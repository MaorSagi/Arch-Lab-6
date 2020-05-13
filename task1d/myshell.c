#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "line_parser.h"
#include "job_control.h"
#include <signal.h>


struct termios* shell_tmodes;
job* cur_job;
pid_t c_pid;
job** job_list;

void signal_handler_d(int sig){ 
    printf("\n");
    kill(c_pid,SIGTSTP);
}

void sig_dfl();
void signal_handler(int sig){ 
    printf("Signal %s was ignored\n",strsignal(sig));
}

void redirect(int old, int new){
    dup2(old,new);
    close(old);
}


void child(cmd_line *line){
    int stream1,stream2;
      if(line->input_redirect != NULL || line->output_redirect != NULL){
        if(line->input_redirect != NULL){
          if((stream1 = open(line->input_redirect,O_RDONLY,0))==-1){
          perror("Error: open");
          exit(1);
        }
        dup2(stream1,0);
        }
        if(line->output_redirect != NULL){
            if((stream2 = open(line->output_redirect,O_RDWR | O_CREAT ,0644))==-1){
            perror("Error: open");
            exit(1);
            }
        dup2(stream2,1);
        }
    }
    int ret = execvp(line->arguments[0],line->arguments);
     if(ret ==-1){
        perror("Error: execvp");
        exit(1);
    }
    if(line->input_redirect != NULL || line->output_redirect != NULL){
        if(line->input_redirect != NULL){
          close(stream1);
        }
        if(line->output_redirect != NULL){
          close(stream2);
        }
    }
}

void execute(cmd_line* line,int* right,int* left,job* job_p,int last_child,pid_t pgid){
    pid_t pid;
    //int status;
    if((pid = fork()) == -1){
            perror("Error: fork");
            exit(1);
    }
    else if(pid ==0){
            sig_dfl();
            if(last_child<0){
                setpgid(getpid(),getpid());
            }
            else{
                setpgid(getpid(),pgid);
            }
            if(right[0] != 0 || right[1] != 0)
                    redirect(right[1],STDOUT_FILENO); 
    
            if(left[0] != 0 || left[1] != 0)
                    redirect(left[0],STDIN_FILENO);
        child(line);
    }
    else{
        cur_job=job_p;
        c_pid=pid;
        signal(SIGTSTP,signal_handler_d);
            if(last_child<0){
                last_child=1;
                job_p->pgid=pid;
                pgid=pid;
                setpgid(getpid(),pid);
            }
            else
                setpgid(getpid(),pgid);
            
            if(right[0] != 0 || right[1] != 0)
                close(right[1]);
            if(left[0] != 0 || left[1] != 0)
                close(left[0]);
            if(line->next!= NULL){
                if((line->next)->next!= NULL)
                     pipe(left);
                else{left[0]=0;left[1]=0;}
            execute(line->next,left,right,job_p,last_child,pgid);
            }
             if(line->blocking == 1){
                //int ret;
                 //while((ret = waitpid(job_p->pgid,&status,WNOHANG))!=-1){
                //}
                run_job_in_foreground (job_list,job_p, 0, shell_tmodes, getpgid(getpid()));
             }
    }
}





void sig_dfl(){
    signal(SIGQUIT,SIG_DFL);
    signal(SIGCHLD,SIG_DFL); 
    signal(SIGTTOU,SIG_DFL);
    signal(SIGTSTP,SIG_DFL);
    signal(SIGTTIN,SIG_DFL);
}


void shell_init(struct termios* shell_tmodes){
    signal(SIGQUIT,signal_handler);
    //signal(SIGCHLD,signal_handler); 
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    setpgid(getpid(),getpid()); //set group id
    tcgetattr(STDIN_FILENO,shell_tmodes); //save default attributer for shell
   
}

int main (int argc, char *argv[]){
  shell_tmodes=malloc(sizeof(struct termios));
  shell_init(shell_tmodes);
  int i = 1; //init job idx
  char input[2048];
  char buf[PATH_MAX];
  job_list=malloc(sizeof(job*));
  getcwd(buf,PATH_MAX);
  printf("%s%c ",buf,'$');
  fgets(input,2048,stdin);
  
  while(strcmp(input,"quit\n")!=0){
    if(input[0]=='\n'||input[0]=='\t'||input[0]==' '){
      strncpy(input,input+1,strlen(input)-1);
       getcwd(buf,PATH_MAX);
    printf("%s%c ",buf,'$');
    fgets(input,2048,stdin);
      continue;
    }
    if(strcmp(input,"")!=0){
        cmd_line* parsed;
        parsed=parse_cmd_lines(input);
        if(!strcmp(parsed->arguments[0],"jobs")){
           print_jobs(job_list); 
        }
        else if(!strcmp(parsed->arguments[0],"fg")){
            int idx =atoi(parsed->arguments[1]);
            job* job_p;
            job_p=find_job_by_index(job_list,idx);
            run_job_in_foreground (job_list,job_p, 1, shell_tmodes, getpgid(getpid()));
        }
        else if(!strcmp(parsed->arguments[0],"bg")){
            int idx =atoi(parsed->arguments[1]);
            job* job_p;
            job_p=find_job_by_index(job_list,idx);
           run_job_in_background(job_p,1);
        }
        else{
        char* cmd = malloc(sizeof(char)*strlen(input));
        job* job_p;
        strcpy(cmd, input);
        
        add_job(job_list,cmd);
        job_p=find_job_by_index(job_list,i);
        i++;
        int last_child=-1;
        int right[2];
        int left[2];
        left[0] = 0;
        left[1] = 0;
        if(parsed->next!=NULL){ // right pipe needed
            pipe(right); 
        }
        execute(parsed,right,left,job_p,last_child,-1);
        free_cmd_lines(parsed);
        }
    }
    
    getcwd(buf,PATH_MAX);
    printf("%s%c ",buf,'$');
    fgets(input,2048,stdin);
}
  
  free_job_list(job_list);
  return 0;

}






