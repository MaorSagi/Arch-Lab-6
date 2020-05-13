#include "job_control.h"
#include "line_parser.h"
#define DONE -1
#define RUNNING 1
#define SUSPENDED 0



/**
* Receive a pointer to a job list and a new command to add to the job list and adds it to it.
* Create a new job list if none exists.
**/
job* add_job(job** job_list, char* cmd){
	job* job_to_add = initialize_job(cmd);
	
	if (*job_list == NULL){
		*job_list = job_to_add;
		job_to_add -> idx = 1;
	}	
	else{
		int counter = 2;
		job* list = *job_list;
		while (list -> next !=NULL){
			printf("adding %d\n", list->idx);
			list = list -> next;
			counter++;
		}
		job_to_add ->idx = counter;
		list -> next = job_to_add;
	}
	return job_to_add;
}
/**
* Receive a pointer to a job list and a pointer to a job and removes the job from the job list 
* freeing its memory.
**/
void remove_job(job** job_list, job* tmp){
	if (*job_list == NULL)
		return;
	job* tmp_list = *job_list;
	if (tmp_list == tmp){
		*job_list = tmp_list -> next;
		free_job(tmp);
		return;
	}
		
	while (tmp_list->next != tmp){
		tmp_list = tmp_list -> next;
	}
	tmp_list -> next = tmp -> next;
	free_job(tmp);
	
}

/**
* receives a status and prints the string it represents.
**/
char* status_to_str(int status)
{
  static char* strs[] = {"Done", "Suspended", "Running"};
  return strs[status + 1];
}


/**
*   Receive a job list, and print it in the following format:<code>[idx] \t status \t\t cmd</code>, where:
    cmd: the full command as typed by the user.
    status: Running, Suspended, Done (for jobs that have completed but are not yet removed from the list).
  
**/
void print_jobs(job** job_list){

	job* tmp = *job_list;
	update_job_list(job_list, FALSE);
	while (tmp != NULL){
		printf("[%d]\t %s \t\t %s", tmp->idx, status_to_str(tmp->status),tmp -> cmd); 
		
		if (tmp -> cmd[strlen(tmp -> cmd)-1]  != '\n')
			printf("\n");
		job* job_to_remove = tmp;
		tmp = tmp -> next;
		if (job_to_remove->status == DONE)
			remove_job(job_list, job_to_remove);
		
	}
 
}


/**
* Receive a pointer to a list of jobs, and delete all of its nodes and the memory allocated for each of them.
*/
void free_job_list(job** job_list){
	while(*job_list != NULL){
		job* tmp = *job_list;
		*job_list = (*job_list) -> next;
		free_job(tmp);
	}
	
}


/**
* receives a pointer to a job, and frees it along with all memory allocated for its fields.
**/
void free_job(job* job_to_remove){
    free(job_to_remove->cmd);                           
    free(job_to_remove->tmodes);
    free(job_to_remove);
    /*if(job_to_remove->next!=NULL){
        job_to_remove->cmd=job_to_remove->next->cmd;
        job_to_remove->pgid=job_to_remove->next->pgid;
        job_to_remove->status=job_to_remove->next->status;
        job_to_remove->tmodes=job_to_remove->next->tmodes;
        job_to_remove->next=job_to_remove->next->next;
        job_to_remove=job_to_remove->next;
        free_job(job_to_remove->next);
    }
    else*/ 	
}



/**
* Receive a command (string) and return a job pointer. 
* The function needs to allocate all required memory for: job, cmd, tmodes
* to copy cmd, and to initialize the rest of the fields to NULL: next, pigd, status 
**/

job* initialize_job(char* cmd){
    job* job_p;
    job_p= malloc(sizeof(job));
    job_p->cmd = malloc(sizeof(char)*1024);
    //job_p->cmd = malloc(sizeof(char)*strlen(cmd));
    strcpy(job_p->cmd, cmd);
    job_p->tmodes=malloc(sizeof(struct termios));
    job_p->pgid = 0;
    job_p->status = RUNNING;
    job_p->next=NULL;	
    return job_p;
}

/**
* Receive a job list and and index and return a pointer to a job with the given index, according to the idx field.
* Print an error message if no job with such an index exists.
**/
job* find_job_by_index(job** job_list, int idx){
    if(job_list==NULL){
        perror("no job with such an index exists");
        return NULL;
    }
    
    job* cur;
    int found=0;
    for(cur=job_list[0] ; cur!=NULL ; cur=cur->next){
        if(cur->idx==idx){
            found=1;
            break;
        }
    }
    if(!found){
        perror("no job with such an index exists");
        return NULL;
    }
    return cur;
}


/**
* Receive a pointer to a job list, and a boolean to decide whether to remove done
* jobs from the job list or not. 
*  This function is used to update the status of jobs running in the background to DONE. Go over all running jobs, and update their status, by waiting for any process in the process group with the option WNOHANG (using waitpid). if remove_done_jobs is set to 1 (TRUE) then DONE jobs are printed in the same format as in print_jobs and are then removed from the job list. WNOHANG does not block the calling process, the process returns from the call to waitpid immediately. If there are no process with the given process group id, then waitpid returns -1.
**/
void update_job_list(job **job_list, int remove_done_jobs){
    job* cur;
    for(cur=job_list[0] ; cur!=NULL ; cur=cur->next){
           waitpid(cur->pgid,&(cur->status),WNOHANG);
            if(remove_done_jobs)
                if(cur->status == DONE){
                    printf("[%d]\t %s \t\t %s", cur->idx, status_to_str(cur->status),cur -> cmd);
                remove_job(job_list,cur);
                    
                }
        
    }
}

/** 
* Put job j in the foreground.  If cont is nonzero, restore the saved terminal modes and send the process group a
* SIGCONT signal to wake it up before we block.  Run update_job_list to print DONE jobs.
**/

void run_job_in_foreground (job** job_list, job *j, int cont, struct termios* shell_tmodes, pid_t shell_pgid){
 int status;
    if( (status =waitpid(j->pgid,&(j->status),WNOHANG))==-1){
     if(j->status == DONE){
                    printf("[%d]\t %s \t\t %s", j->idx, status_to_str(j->status),j -> cmd);
                remove_job(job_list,j);
    }
    }
    else{
        tcsetpgrp(STDIN_FILENO,j->pgid);
        if(cont!=0 && j->status==SUSPENDED)
            tcsetattr(STDIN_FILENO,TCSADRAIN,j->tmodes);
        kill(j->pgid,SIGCONT);
        waitpid(j->pgid,&status,WUNTRACED|WSTOPPED);
        if(WIFSTOPPED(status)) j->status=SUSPENDED; 
        if(WIFEXITED(status) || WIFSIGNALED(status)) j->status=DONE;

        tcsetpgrp(STDIN_FILENO,shell_pgid);//shell back in fg
        tcgetattr(STDIN_FILENO,j->tmodes); //save attributer for job
        tcsetattr(STDIN_FILENO,TCSADRAIN,shell_tmodes); //attributer for shell
        update_job_list(job_list,1);
    }
}

/** 
* Put a job in the background.  If the cont argument is nonzero, send
* the process group a SIGCONT signal to wake it up.  
**/

void run_job_in_background (job *j, int cont){	
    if(cont!=0){   
    j->status=RUNNING;
        kill(j->pgid,SIGCONT);
    }
}
