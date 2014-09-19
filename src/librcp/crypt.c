/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include "librcp.h"

static unsigned char itoa64[] =			  /* 0 ... 63 => ascii - 64 */
"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void to64(char *s, unsigned long v, int n) {
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}


/*
 * UNIX password
 *
 * Use MD5 for what it is best at...
 */

char *rcpCrypt(const char *pw, const char *salt) {
	static char     *magic = "$1$";
	/*
	 * This string is magic for
	 * this algorithm.  Having
	 * it this way, we can get
	 * get better later on
	 */
	static char     passwd[120], *p;
	static const char *sp,*ep;
	unsigned char   final[16];
	int sl,pl,i,j;
	MD5_CTX ctx,ctx1;
	unsigned long l;

	/* Refine the Salt first */
	sp = salt;

	/* If it starts with the magic string, then skip that */
	if(!strncmp(sp,magic,strlen(magic)))
		sp += strlen(magic);

	/* It stops at the first '$', max 8 chars */
	for(ep=sp;*ep && *ep != '$' && ep < (sp+8);ep++)
		continue;

	/* get the length of the true salt */
	sl = ep - sp;

	MD5Init(&ctx);

	/* The password first, since that is what is most unknown */
	MD5Update(&ctx,(unsigned char *)pw,strlen(pw));

	/* Then our magic string */
	MD5Update(&ctx,(unsigned char *)magic,strlen(magic));

	/* Then the raw salt */
	MD5Update(&ctx,(unsigned char *)sp,sl);

	/* Then just as many characters of the MD5(pw,salt,pw) */
	MD5Init(&ctx1);
	MD5Update(&ctx1,(unsigned char *)pw,strlen(pw));
	MD5Update(&ctx1,(unsigned char *)sp,sl);
	MD5Update(&ctx1,(unsigned char *)pw,strlen(pw));
	MD5Final(final,&ctx1);
	for(pl = strlen(pw); pl > 0; pl -= 16)
		MD5Update(&ctx,(unsigned char *)final,pl>16 ? 16 : pl);

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	/* Then something really weird... */
	for (j=0,i = strlen(pw); i ; i >>= 1)
		if(i&1)
			MD5Update(&ctx, (unsigned char *)final+j, 1);
	else
		MD5Update(&ctx, (unsigned char *)pw+j, 1);

	/* Now make the output string */
	strcpy(passwd,magic);
	strncat(passwd,sp,sl);
	strcat(passwd,"$");

	MD5Final(final,&ctx);

	/* 
	 * and now, just to make sure things don't run too fast
	 * On a 60 Mhz Pentium this takes 34 msec, so you would
	 * need 30 seconds to build a 1000 entry dictionary...
	 */
	for(i=0;i<1000;i++) {
		MD5Init(&ctx1);
		if(i & 1)
			MD5Update(&ctx1,(unsigned char *)pw,strlen(pw));
		else
			MD5Update(&ctx1,(unsigned char *)final,16);

		if(i % 3)
			MD5Update(&ctx1,(unsigned char *)sp,sl);

		if(i % 7)
			MD5Update(&ctx1,(unsigned char *)pw,strlen(pw));

		if(i & 1)
			MD5Update(&ctx1,(unsigned char *)final,16);
		else
			MD5Update(&ctx1,(unsigned char *)pw,strlen(pw));
		MD5Final(final,&ctx1);
	}

	p = passwd + strlen(passwd);

	l = (final[ 0]<<16) | (final[ 6]<<8) | final[12]; to64(p,l,4); p += 4;
	l = (final[ 1]<<16) | (final[ 7]<<8) | final[13]; to64(p,l,4); p += 4;
	l = (final[ 2]<<16) | (final[ 8]<<8) | final[14]; to64(p,l,4); p += 4;
	l = (final[ 3]<<16) | (final[ 9]<<8) | final[15]; to64(p,l,4); p += 4;
	l = (final[ 4]<<16) | (final[10]<<8) | final[ 5]; to64(p,l,4); p += 4;
	l =                    final[11]                ; to64(p,l,2); p += 2;
	*p = '\0';

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	return passwd;
}

#if 0
int main(void) {
	char *hash;
	
	// store hash
	char *passwd = "12345678";
	{
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++) {
			unsigned s = ((unsigned) rand()) % ((unsigned) 'Z' - (unsigned) 'A');
			solt[i] = 'A' + s;
		}
		solt[RCP_CRYPT_SALT_LEN] = '\0';
		
		hash = rcpCrypt(passwd, solt);
		printf("passwd %s, hash %s\n", passwd, hash + 3);
//		ASSERT(strlen(hash + 3) <= CLI_MAX_PASSWD);
//		memset(shm->config.enable_passwd, 0, CLI_MAX_PASSWD + 1);
//		strncpy(shm->config.enable_passwd, hash + 3, CLI_MAX_PASSWD);
	}

	 // check password
	{
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++)
			solt[i] = (hash + 3)[i];	//shm->config.enable_passwd[i];
		solt[RCP_CRYPT_SALT_LEN] = '\0';

		char *cp = rcpCrypt(passwd, solt);
//		if (strcmp(shm->config.enable_passwd + RCP_CRYPT_SALT_LEN + 1, cp + 4 + RCP_CRYPT_SALT_LEN) != 0) {
//			printf("Error: invalid password\n");
//			return RCPERR;
//		}
		printf("passwd %s, hash %s\n", passwd, cp + 3);
	}


	return 0;
}
#endif

