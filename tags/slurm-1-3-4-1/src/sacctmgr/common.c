/*****************************************************************************\
 *  common.c - definitions for functions common to all modules in sacctmgr.
 *****************************************************************************
 *  Copyright (C) 2008 Lawrence Livermore National Security.
 *  Copyright (C) 2002-2007 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Danny Auble <da@llnl.gov>
 *  LLNL-CODE-402394.
 *  
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.llnl.gov/linux/slurm/>.
 *  
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission 
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and 
 *  distribute linked combinations including the two. You must obey the GNU 
 *  General Public License in all respects for all of the code used other than 
 *  OpenSSL. If you modify file(s) with this exception, you may extend this 
 *  exception to your version of the file(s), but you are not obligated to do 
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in 
 *  the program, then also delete it here.
 *  
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include "src/sacctmgr/sacctmgr.h"
#include <unistd.h>
#include <termios.h>

#define FORMAT_STRING_SIZE 32

static pthread_t lock_warning_thread;

static void *_print_lock_warn(void *no_data)
{
	sleep(2);
	printf(" Waiting for lock from other user.\n");

	return NULL;
}

static void nonblock(int state)
{
	struct termios ttystate;

	//get the terminal state
	tcgetattr(STDIN_FILENO, &ttystate);

	switch(state) {
	case 1:
		//turn off canonical mode
		ttystate.c_lflag &= ~ICANON;
		//minimum of number input read.
		ttystate.c_cc[VMIN] = 1;
		break;
	default:
		//turn on canonical mode
		ttystate.c_lflag |= ICANON;
	}
	//set the terminal attributes.
	tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

}

extern int parse_option_end(char *option)
{
	int end = 0;
	
	if(!option)
		return 0;

	while(option[end] && option[end] != '=')
		end++;
	if(!option[end])
		return 0;
	end++;
	return end;
}

/* you need to xfree whatever is sent from here */
extern char *strip_quotes(char *option, int *increased)
{
	int end = 0;
	int i=0, start=0;
	char *meat = NULL;

	if(!option)
		return NULL;

	/* first strip off the ("|')'s */
	if (option[i] == '\"' || option[i] == '\'')
		i++;
	start = i;

	while(option[i]) {
		if(option[i] == '\"' || option[i] == '\'') {
			end++;
			break;
		}
		i++;
	}
	end += i;

	meat = xmalloc((i-start)+1);
	memcpy(meat, option+start, (i-start));

	if(increased)
		(*increased) += end;

	return meat;
}

extern void addto_char_list(List char_list, char *names)
{
	int i=0, start=0;
	char *name = NULL, *tmp_char = NULL;
	ListIterator itr = list_iterator_create(char_list);

	if(names && char_list) {
		if (names[i] == '\"' || names[i] == '\'')
			i++;
		start = i;
		while(names[i]) {
			if(names[i] == '\"' || names[i] == '\'')
				break;
			else if(names[i] == ',') {
				if((i-start) > 0) {
					name = xmalloc((i-start+1));
					memcpy(name, names+start, (i-start));

					while((tmp_char = list_next(itr))) {
						if(!strcasecmp(tmp_char, name))
							break;
					}

					if(!tmp_char)
						list_append(char_list, name);
					else 
						xfree(name);
					list_iterator_reset(itr);
				}
				i++;
				start = i;
			}
			i++;
		}
		if((i-start) > 0) {
			name = xmalloc((i-start)+1);
			memcpy(name, names+start, (i-start));
			while((tmp_char = list_next(itr))) {
				if(!strcasecmp(tmp_char, name))
					break;
			}
			
			if(!tmp_char)
				list_append(char_list, name);
			else 
				xfree(name);
		}
	}	
	list_iterator_destroy(itr);
} 

extern int notice_thread_init()
{
	pthread_attr_t attr;
	
	slurm_attr_init(&attr);
	if(pthread_create(&lock_warning_thread, &attr, &_print_lock_warn, NULL))
		error ("pthread_create error %m");
	slurm_attr_destroy(&attr);
	return SLURM_SUCCESS;
}

extern int notice_thread_fini()
{
	return pthread_cancel(lock_warning_thread);
}

extern int commit_check(char *warning) 
{
	int ans = 0;
	char c = '\0';
	int fd = fileno(stdin);
	fd_set rfds;
	struct timeval tv;

	if(!rollback_flag)
		return 1;

	printf("%s (You have 30 seconds to decide)\n", warning);
	nonblock(1);
	while(c != 'Y' && c != 'y'
	      && c != 'N' && c != 'n'
	      && c != '\n') {
		if(c) {
			printf("Y or N please\n");
		}
		printf("(N/y): ");
		fflush(stdout);
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		/* Wait up to 30 seconds. */
		tv.tv_sec = 30;
		tv.tv_usec = 0;
		if((ans = select(fd+1, &rfds, NULL, NULL, &tv)) <= 0)
			break;
		
		c = getchar();
		printf("\n");
	}
	nonblock(0);
	if(ans <= 0) 
		printf("timeout\n");
	else if(c == 'Y' || c == 'y') 
		return 1;			
	
	return 0;
}

extern acct_association_rec_t *sacctmgr_find_association(char *user,
							 char *account,
							 char *cluster,
							 char *partition)
{
	acct_association_rec_t * assoc = NULL;
	acct_association_cond_t assoc_cond;
	List assoc_list = NULL;

	memset(&assoc_cond, 0, sizeof(acct_association_cond_t));
	if(account) {
		assoc_cond.acct_list = list_create(NULL);
		list_append(assoc_cond.acct_list, account);
	} else {
		error("need an account to find association");
		return NULL;
	}
	if(cluster) {
		assoc_cond.cluster_list = list_create(NULL);
		list_append(assoc_cond.cluster_list, cluster);
	} else {
		if(assoc_cond.acct_list)
			list_destroy(assoc_cond.acct_list);
		error("need an cluster to find association");
		return NULL;
	}

	assoc_cond.user_list = list_create(NULL);
	if(user) 
		list_append(assoc_cond.user_list, user);
	else
		list_append(assoc_cond.user_list, "");
	
	assoc_cond.partition_list = list_create(NULL);
	if(partition) 
		list_append(assoc_cond.partition_list, partition);
	else
		list_append(assoc_cond.partition_list, "");
	
	assoc_list = acct_storage_g_get_associations(db_conn, &assoc_cond);
	
	list_destroy(assoc_cond.acct_list);
	list_destroy(assoc_cond.cluster_list);
	list_destroy(assoc_cond.user_list);
	list_destroy(assoc_cond.partition_list);

	if(assoc_list)
		assoc = list_pop(assoc_list);

	list_destroy(assoc_list);
	
	return assoc;
}

extern acct_association_rec_t *sacctmgr_find_account_base_assoc(char *account,
								char *cluster)
{
	acct_association_rec_t *assoc = NULL;
	char *temp = "root";
	acct_association_cond_t assoc_cond;
	List assoc_list = NULL;

	if(!cluster)
		return NULL;

	if(account)
		temp = account;

	memset(&assoc_cond, 0, sizeof(acct_association_cond_t));
	assoc_cond.acct_list = list_create(NULL);
	list_append(assoc_cond.cluster_list, temp);
	assoc_cond.cluster_list = list_create(NULL);
	list_append(assoc_cond.cluster_list, cluster);
	assoc_cond.user_list = list_create(NULL);
	list_append(assoc_cond.user_list, "");

//	info("looking for %s %s in %d", account, cluster,
//	     list_count(sacctmgr_association_list));
	
	assoc_list = acct_storage_g_get_associations(db_conn, &assoc_cond);

	list_destroy(assoc_cond.acct_list);
	list_destroy(assoc_cond.cluster_list);
	list_destroy(assoc_cond.user_list);

	if(assoc_list)
		assoc = list_pop(assoc_list);

	list_destroy(assoc_list);

	return assoc;
}

extern acct_association_rec_t *sacctmgr_find_root_assoc(char *cluster)
{
	return sacctmgr_find_account_base_assoc(NULL, cluster);
}

extern acct_user_rec_t *sacctmgr_find_user(char *name)
{
	acct_user_rec_t *user = NULL;
	acct_user_cond_t user_cond;
	List user_list = NULL;
	
	if(!name)
		return NULL;
	
	memset(&user_cond, 0, sizeof(acct_user_cond_t));
	user_cond.user_list = list_create(NULL);
	list_append(user_cond.user_list, name);

	user_list = acct_storage_g_get_users(db_conn, &user_cond);

	list_destroy(user_cond.user_list);

	if(user_list)
		user = list_pop(user_list);

	list_destroy(user_list);

	return user;
}

extern acct_account_rec_t *sacctmgr_find_account(char *name)
{
	acct_account_rec_t *account = NULL;
	acct_account_cond_t account_cond;
	List account_list = NULL;
	
	if(!name)
		return NULL;

	memset(&account_cond, 0, sizeof(acct_account_cond_t));
	account_cond.acct_list = list_create(NULL);
	list_append(account_cond.acct_list, name);

	account_list = acct_storage_g_get_accounts(db_conn, &account_cond);
	
	list_destroy(account_cond.acct_list);

	if(account_list)
		account = list_pop(account_list);

	list_destroy(account_list);

	return account;
}

extern acct_cluster_rec_t *sacctmgr_find_cluster(char *name)
{
	acct_cluster_rec_t *cluster = NULL;
	acct_cluster_cond_t cluster_cond;
	List cluster_list = NULL;

	if(!name)
		return NULL;

	memset(&cluster_cond, 0, sizeof(acct_cluster_cond_t));
	cluster_cond.cluster_list = list_create(NULL);
	list_append(cluster_cond.cluster_list, name);

	cluster_list = acct_storage_g_get_clusters(db_conn, &cluster_cond);

	list_destroy(cluster_cond.cluster_list);

	if(cluster_list) 
		cluster = list_pop(cluster_list);

	list_destroy(cluster_list);
	
	return cluster;
}

extern acct_association_rec_t *sacctmgr_find_association_from_list(
	List assoc_list, char *user, char *account, 
	char *cluster, char *partition)
{
	ListIterator itr = NULL;
	acct_association_rec_t * assoc = NULL;
	
	if(!assoc_list)
		return NULL;
	
	itr = list_iterator_create(assoc_list);
	while((assoc = list_next(itr))) {
		if((user && (!assoc->user || strcasecmp(user, assoc->user)))
		   || (account && (!assoc->acct 
				   || strcasecmp(account, assoc->acct)))
		   || (cluster && (!assoc->cluster 
				   || strcasecmp(cluster, assoc->cluster)))
		   || (partition && (!assoc->partition 
				     || strcasecmp(partition, 
						   assoc->partition))))
			continue;
		break;
	}
	list_iterator_destroy(itr);
	
	return assoc;
}

extern acct_association_rec_t *sacctmgr_find_account_base_assoc_from_list(
	List assoc_list, char *account, char *cluster)
{
	ListIterator itr = NULL;
	acct_association_rec_t *assoc = NULL;
	char *temp = "root";

	if(!cluster || !assoc_list)
		return NULL;

	if(account)
		temp = account;
	/* info("looking for %s %s in %d", account, cluster, */
/* 	     list_count(assoc_list)); */
	itr = list_iterator_create(assoc_list);
	while((assoc = list_next(itr))) {
		/* info("is it %s %s %s", assoc->user, assoc->acct, assoc->cluster); */
		if(assoc->user
		   || strcasecmp(temp, assoc->acct)
		   || strcasecmp(cluster, assoc->cluster))
			continue;
	/* 	info("found it"); */
		break;
	}
	list_iterator_destroy(itr);

	return assoc;
}
extern acct_user_rec_t *sacctmgr_find_user_from_list(
	List user_list, char *name)
{
	ListIterator itr = NULL;
	acct_user_rec_t *user = NULL;
	
	if(!name || !user_list)
		return NULL;
	
	itr = list_iterator_create(user_list);
	while((user = list_next(itr))) {
		if(!strcasecmp(name, user->name))
			break;
	}
	list_iterator_destroy(itr);
	
	return user;

}

extern acct_account_rec_t *sacctmgr_find_account_from_list(
	List acct_list, char *name)
{
	ListIterator itr = NULL;
	acct_account_rec_t *account = NULL;
	
	if(!name || !acct_list)
		return NULL;

	itr = list_iterator_create(acct_list);
	while((account = list_next(itr))) {
		if(!strcasecmp(name, account->name))
			break;
	}
	list_iterator_destroy(itr);
	
	return account;

}

extern acct_cluster_rec_t *sacctmgr_find_cluster_from_list(
	List cluster_list, char *name)
{
	ListIterator itr = NULL;
	acct_cluster_rec_t *cluster = NULL;

	if(!name || !cluster_list)
		return NULL;

	itr = list_iterator_create(cluster_list);
	while((cluster = list_next(itr))) {
		if(!strcasecmp(name, cluster->name))
			break;
	}
	list_iterator_destroy(itr);
	
	return cluster;
}

extern int get_uint(char *in_value, uint32_t *out_value, char *type)
{
	char *ptr = NULL, *meat = NULL;
	long num;
	
	if(!(meat = strip_quotes(in_value, NULL)))
		return SLURM_ERROR;

	num = strtol(meat, &ptr, 10);
	if ((num == 0) && ptr && ptr[0]) {
		error("Invalid value for %s (%s)", type, meat);
		xfree(meat);
		return SLURM_ERROR;
	}
	xfree(meat);
	
	if (num < 0)
		*out_value = INFINITE;		/* flag to clear */
	else
		*out_value = (uint32_t) num;
	return SLURM_SUCCESS;
}
