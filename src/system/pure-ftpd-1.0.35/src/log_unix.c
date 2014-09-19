//Modified for Rcp Project
#include <config.h>

#include "ftpd.h"
#include "log_unix.h"

#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif

//RCP
#include "../../../include/librcp.h"
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

// debug on /var/log/messages
static void dbgmsg(const char *msg) {
    logfile(LOG_INFO, "%s", msg);
}
//RCP

void pw_unix_check(AuthResult * const result,
                   const char *account, const char *password,
                   const struct sockaddr_storage * const sa,
                   const struct sockaddr_storage * const peer)
{

//RCP - check user and password
	dbgmsg("checking user/password");
	if (result == NULL || password == NULL || strlen(password) == 0)
		return;

	// initialize shared memory
	RcpShm *shm;
	if ((shm = rcpShmemInit(RCP_PROC_CLI)) == NULL) {
		dbgmsg("cannot initialize memory, exiting...\n");
		return;
	}
	RcpAdmin *admin = admin_find(shm, account);
	if (admin == NULL) {
		dbgmsg("failed to find an RCP administrator account, exiting...\n");
		return;
	}

	{ // check password
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++)
			solt[i] = admin->password[i];
		solt[RCP_CRYPT_SALT_LEN] = '\0';

		char *cp = rcpCrypt(password, solt);
		if (strcmp(admin->password + RCP_CRYPT_SALT_LEN + 1, cp + RCP_CRYPT_SALT_LEN + 4) != 0)
			return;
	}
	// user/password combination is ok
	// for the login into /home/rcp, with rcp as the real user
	account = "rcp";
//RCP


    const char *cpwd = NULL;
    struct passwd pw, *pw_;
#ifdef USE_SHADOW
    struct spwd *spw;
#endif
    char *dir = NULL;

    (void) sa;
    (void) peer;
    result->auth_ok = 0;
    if ((pw_ = getpwnam(account)) == NULL) {
/*RCP*/dbgmsg("failed getpwnam");    	
        return;
    }
    pw = *pw_;
    result->auth_ok--;
#ifdef HAVE_SETUSERSHELL
    if (pw.pw_shell == NULL) {
/*RCP*/dbgmsg("failed pw_shell");    	
        return;
    }
#if 0 // RCP disabled
    if (strcasecmp(pw.pw_shell, FAKE_SHELL) != 0) {
        const char *shell;
        
        setusershell();
        while ((shell = (char *) getusershell()) != NULL &&
               strcmp(pw.pw_shell, shell) != 0);
        endusershell();
        if (shell == NULL) {
            return;
        }
    }
#endif //RCP      
#endif
    if ((dir = strdup(pw.pw_dir)) == NULL) {
/*RCP*/dbgmsg("failed strdup");    	
        return;
    }
#if 0 //disabeld for RCP
#ifdef USE_SHADOW
    if ((((pw.pw_passwd)[0] == 'x' && (pw.pw_passwd)[1] == 0) ||
         ((pw.pw_passwd)[0] == '#' && (pw.pw_passwd)[1] == '#' &&
          strcmp(pw.pw_passwd + 2, account) == 0)) &&
        (spw = getspnam(account)) != NULL && spw->sp_pwdp != NULL) {
        cpwd = spw->sp_pwdp[0] == '@' ? NULL : spw->sp_pwdp;
        if (spw->sp_expire > 0 || spw->sp_max > 0) {
            long today = time(NULL) / (24L * 60L * 60L);

            if (spw->sp_expire > 0 && spw->sp_expire < today) {
                goto bye;               /* account expired */
            }
            if (spw->sp_max > 0 && spw->sp_lstchg > 0 &&
                (spw->sp_lstchg + spw->sp_max < today)) {
                goto bye;               /* password expired */
            }
        }
    } else
#endif
    {
        cpwd = pw.pw_passwd;
    }
    {
        const char *crypted;
        
        if (cpwd == NULL ||
            (crypted = (const char *) crypt(password, cpwd)) == NULL ||
            strcmp(cpwd, crypted) != 0) {
            goto bye;
        }
    }
#endif //RCP
    result->uid = pw.pw_uid;
    result->gid = pw.pw_gid;
    result->dir = dir;
    result->slow_tilde_expansion = 0;
    result->auth_ok = -result->auth_ok;
/*RCP*/dbgmsg("RCP admin check ok");    	
    return;
    
    bye:
    free(dir);
/*RCP*/dbgmsg("failed RCP admin check");    	
}

