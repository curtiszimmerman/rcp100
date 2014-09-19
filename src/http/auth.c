#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "http.h"

#define MAX_BUF 2048
static char buf[MAX_BUF + 1];

static RcpAdmin *admin_find(RcpShm *shm, const char *name) {
	int i;
	RcpAdmin *ptr;

	for (i = 0, ptr = shm->config.admin; i < RCP_ADMIN_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		if (strcmp(ptr->name, name) == 0)
			return ptr;
	}
	
	return NULL;
}


static RcpHttpAuth *find_http_auth(uint32_t ip) {
	int i;
	for (i = 0; i < RCP_HTTP_AUTH_LIMIT; i++) {
		if (shm->stats.http_auth[i].ip == ip)
			return &shm->stats.http_auth[i];
	}
	return NULL;
}

// HMAC-MD5 implementation based on RFC2104
static void hmac_md5(unsigned char*  text, int text_len, unsigned char* key, int key_len, unsigned char *digest) {
	MD5_CTX context;
	unsigned char k_ipad[65];
	/* inner padding -
	 * key XORd with ipad
	 */
	unsigned char k_opad[65];
	/* outer padding -
	 * key XORd with opad
	 */
	unsigned char tk[16];
	int i;
	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {

		MD5_CTX      tctx;

		MD5Init(&tctx);
		MD5Update(&tctx, key, key_len);
		MD5Final(tk, &tctx);

		key = tk;
		key_len = 16;
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	bzero( k_ipad, sizeof k_ipad);
	bzero( k_opad, sizeof k_opad);
	bcopy( key, k_ipad, key_len);
	bcopy( key, k_opad, key_len);

	/* XOR key with ipad and opad values */
	for (i=0; i<64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}
	/*
	 * perform inner MD5
	 */
	MD5Init(&context);
	/* init context for 1st
	 * pass */
	MD5Update(&context, k_ipad, 64);	  /* start with inner pad */
	MD5Update(&context, text, text_len);	  /* then text of datagram */
	MD5Final(digest, &context);		  /* finish up 1st pass */
	/*
	 * perform outer MD5
	 */
	MD5Init(&context);
	/* init context for 2nd
	 * pass */
	MD5Update(&context, k_opad, 64);	  /* start with outer pad */
	MD5Update(&context, digest, 16);
	/* then results of 1st
	 * hash */
	MD5Final(digest, &context);		  /* finish up 2nd pass */
}


//***********************************************************************
// User authentication - from the login page
//***********************************************************************
#define ERR_PASSWD "Invalid user name/password."
void auth_user(uint32_t ip, char *line) {
	char *user = strstr(line, "username");
	char *passwd = strstr(line, "password");
	if (user == NULL || passwd == NULL)
		send_error( 400, "Bad Request", (char*) 0, ERR_PASSWD);
	user += 9;
	passwd += 9;
	char *ptr = strchr(user, '&');
	if (ptr)
		*ptr = '\0';
	ptr = strchr(passwd, '&');
	if (ptr)
		*ptr = '\0';
	
	RcpAdmin *admin = admin_find(shm, user);
	if (admin == NULL)
		send_error( 400, "Bad Request", (char*) 0, ERR_PASSWD);
	{ // check password
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++)
			solt[i] = admin->password[i];
		solt[RCP_CRYPT_SALT_LEN] = '\0';

		char *cp = rcpCrypt(passwd, solt);
		if (strcmp(admin->password + RCP_CRYPT_SALT_LEN + 1, cp + RCP_CRYPT_SALT_LEN + 4) != 0) {
			send_error( 400, "Bad Request", (char*) 0, ERR_PASSWD);
		}
	}
	
	// connect to rcp services and send the user IP address to netmon
	int muxsock =  rcpConnectMux(RCP_PROC_TESTPROC, NULL, NULL, NULL);	
	if (muxsock == 0)
		send_error( 400, "Bad Request", (char*) 0, ERR_PASSWD);
	
	RcpPkt pkt;
	memset(&pkt, 0, sizeof(pkt));
	pkt.destination = RCP_PROC_SERVICES;
	pkt.type = RCP_PKT_TYPE_HTTP_AUTH;
	pkt.pkt.http_auth.ip = ip;
	
	errno = 0;
	int n = send(muxsock, &pkt, sizeof(pkt), 0);
	if (n < 1) {
		close(muxsock);
		send_error( 400, "Bad Request", (char*) 0, ERR_PASSWD);
	}
	close(muxsock);
	
	// forward index.html file
	send_headers( 200, "Ok", (char*) 0, "text/html", -1, -1);
	printf("\n");
	unsigned sid = get_session();
	
	// check default passwords
//printf("%d, %d, %s\n", shm->config.admin_default_passwd, shm->config.http_default_passwd, shm->config.http_passwd);	
//fflush(0);
	if (shm->config.admin_default_passwd &&
	     shm->config.http_default_passwd &&
	     shm->config.http_passwd[0] != '\0')
	     	html_redir_passwd(sid);
	else {
		html_redir(sid);
	}
}

//***********************************************************************
// rest/token.json
// get a security token and password salt
//***********************************************************************
static void print_token(const char *salt, uint32_t token) {
	buf[0] = '\0';
	char *ptr = buf;
	int len = 0;
	
	// result
	int rv = sprintf(ptr, "{\n\t\"result\": true,\n");
	ptr += rv;
	len += rv;
	
	// salt
	rv = sprintf(ptr, "\t\"solt\": \"%s\",\n", salt);
	ptr += rv;
	len += rv;
	
	// token
	rv = sprintf(ptr, "\t\"token\": \"%x\"\n}\n", token);
	len += rv;
	
	send_headers( 200, "Ok", (char*) 0, "application/json", len, -1);
	printf("%s", buf);
	http_debug(RLOG_DEBUG, "send token:", buf);

	fflush(0);
}

void auth_rest_token(uint32_t ip) {
	// if a http password is not configured don't bother to respond
	if (shm->config.http_passwd[0] == '\0')
		return;
	
	// extract the solt
	char solt[RCP_CRYPT_SALT_LEN + 1];
	int i;
	for (i = 0; i < RCP_CRYPT_SALT_LEN; i++)
		solt[i] = shm->config.http_passwd[i];
	solt[RCP_CRYPT_SALT_LEN] = '\0';

	RcpHttpAuth *auth = find_http_auth(ip);
	if (auth) {
		print_token(solt, auth->token);
		return;
	}

	// connect to rcp services and send the user IP address to netmon
	int muxsock =  rcpConnectMux(RCP_PROC_TESTPROC, NULL, NULL, NULL);	
	if (muxsock == 0)
		send_error( 400, "Bad Request", (char*) 0, "Illegal filename." );
	
	RcpPkt pkt;
	memset(&pkt, 0, sizeof(pkt));
	pkt.destination = RCP_PROC_SERVICES;
	pkt.type = RCP_PKT_TYPE_HTTP_AUTH;
	pkt.pkt.http_auth.ip = ip;
	
	// send token request
	errno = 0;
	int n = send(muxsock, &pkt, sizeof(pkt), 0);
	if (n < 1) {
		close(muxsock);
		send_error( 400, "Bad Request", (char*) 0, "Illegal filename." );
	}
	fflush(0);
	close(muxsock);

	// wait 1 second and check if the token was updated
	sleep(1);
	
	// get token response
	auth = find_http_auth(ip);
	if (auth) {
		print_token(solt, auth->token);
	}
}

//***********************************************************************
// rest/command.json
// execute a cli command in config mode
//***********************************************************************
static void command_error(const char *msg) {
	buf[0] = '\0';
	char *ptr = buf;
	int len = 0;

	http_debug(RLOG_INFO, "http rest error:", msg);
	
	int rv = sprintf(ptr, "Error: %s\n", msg);
	ptr += rv;
	len += rv;

	send_headers( 200, "Ok", (char*) 0, "text/plain", len, -1);
	
	printf("%s", buf);
	fflush(0);
}

static void command_successful(const char *output_fname) {
	// command output
	struct stat s;
	memset(&s, 0, sizeof(struct stat));
	FILE *fp = fopen(output_fname, "r");
	if (fp)
		stat(output_fname, &s);

	send_headers( 200, "Ok", (char*) 0, "text/plain", s.st_size + 4, -1);

	printf("%s", buf);
	if (fp) {
		while (fgets(buf, MAX_BUF, fp))
			printf("%s", buf);
		fclose(fp);
	}

	printf("end\n");
	fflush(0);
}

void auth_rest_command(uint32_t ip, char *line) {
	// store the line
	char *storage = strdup(line);

//printf("received #%s#\n", storage);	

	// if a http password is not configured don't bother to respond
	if (shm->config.http_passwd[0] == '\0') {
		command_error("password not configured");
		return;
	}

	// match ip address
	RcpHttpAuth *auth = find_http_auth(ip);
	if (!auth) {
		command_error("no security token generated for this session");
		return;
	}

	// extract tokens
	char *token = NULL;
	char *hmac = NULL;
	char *command = NULL;

	// parse the line
	char *ptr = line;
	while ((ptr = strstr(ptr, "&&")) != NULL) {
		*ptr++ = '\n';
		*ptr++ = ' ';
	}
	ptr = line;
	while (ptr) {
		char *ptr1 = strchr(ptr, '&');

		if (ptr1) {
			*ptr1 = '\0';
			ptr1++;
		}
		
		// parse element
		if (strncmp(ptr, "token=", 6) == 0) {
			ptr += 6;
			token = strdup(ptr);
		}
		else if (strncmp(ptr, "hmac=", 5) == 0) {
			ptr += 5;
			hmac = strdup(ptr);
		}
		if (strncmp(ptr, "command=", 8) == 0) {
			ptr += 8;
			command = strdup(ptr);
		}
		
		
		ptr = ptr1;
	}
	

	if (!token) {
		command_error("security token missing");
		return;
	}
	if (!hmac) {
		command_error("hmac missing");
		return;
	}
	if (!command) {
		command_error("command missing");
		return;
	}

	// verify security token
	unsigned stoken = 0;
	sscanf(token, "%x", &stoken);
	if (stoken != auth->token) {
		command_error("security token not matching");
		return;
	}

	// build key
	char *key = malloc(strlen(shm->config.http_passwd));
 	strcpy(key, shm->config.http_passwd + RCP_CRYPT_SALT_LEN + 1);
//printf("key #%s#\n", key);
 
	// calculate hmac
	ptr = strstr(storage, "hmac=");
	if (!ptr) {
		command_error("cannot parse request");
		return;
	}
	*(ptr - 1) = '\0';
	
	unsigned char digest[16];
	unsigned char digest_str[16 * 2 + 1];
	digest_str[32] = '\0';
	hmac_md5((unsigned char *) storage, strlen(storage), (unsigned char *) key, strlen(key), digest);
	int i;
	for (i = 0; i < 16; i++)
		sprintf((char *)digest_str + i * 2, "%02x", digest[i] & 0xff);
	
	// compare hmac digest
//printf("hmac sent %s, hmac calculated %s\n", hmac, digest_str);	
	if (strcmp(hmac, (char *) digest_str) != 0)
		command_error("authentication failure");
		
	// store the command in a temporary file and pass it to cli
	char fname1[50] = "/opt/rcp/var/transport/hXXXXXX";
	int fd = mkstemp(fname1);
	if (fd != -1) {
		close(fd);
		FILE *fp = fopen(fname1, "w+");
		if (fp) {
			// replace && with "\n "
			fprintf(fp, "%s\n", command);
			fclose(fp);
			
			char fname2[50] = "/opt/rcp/var/transport/hXXXXXX";
			int fd = mkstemp(fname2);
			if (fd != -1) {
				close(fd);
				char cmd[200];
				sprintf(cmd, "/opt/rcp/bin/cli --html --script %s > %s", fname1, fname2);
				int v = system(cmd);
				(void) v;
				command_successful(fname2);
				unlink(fname2);
			}
		}
		unlink(fname1);
		
	}
	else 
		command_error("cannot run command");
	
	// reset authentication token
	srand(time(NULL));
	auth->token = (unsigned) rand();
	http_debug(RLOG_DEBUG, "renew authentication token", "");
}
