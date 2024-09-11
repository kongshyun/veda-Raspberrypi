#include "server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>// O_NONBLOCK 플래그를 사용하기 위한 헤더 파일

#define TCP_PORT 5100 /*서버의 포트 번호*/
#define MAX_CLIENTS 10

//클라이언트 정보 구조체
typedef struct{
    int sockfd;         //클라이언트 소켓 파일 디스크립터
    char username[50];  //클라이언트 이름
}Client;

Client clients[MAX_CLIENTS]; //클라이언트 리스트
int client_count =0;
int running_children=0; // 실행중인 자식 프로세스 수

//메시지 구조체
typedef struct{
    char type[10];  //메시지 타입("LOGIN","MSG")
    char username[50];//사용자이름
    char content[256];//메시지 내용 
}Message;

int pipefd[2]; //[0]읽기 끝, [1]쓰기 끝 


void broadcast_message(Message *msg, int sender_sock) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sockfd != sender_sock) {
            send(clients[i].sockfd, msg, sizeof(*msg), 0);
        }
    }
}

//자식 프로세스가 종료되었을때 처리
void handle_sigchld(int signum){
    int status;
    pid_t pid;

    while((pid=waitpid(-1, &status, WNOHANG))>0){
        running_children--; // 자식프로세스 수 감소 
        printf("Child process PID %d :죽음, (남은자식수 : %d)\n", pid,running_children);

        //남은 자식이 없으면 부모도 종료 
        if(running_children ==0){
            printf("All child process terminated, Shutdown server.\n");
            close(pipefd[0]);
            close(pipefd[1]);
            exit(0);
        }
    }
}

//파이프에서 메시지를 읽고 처리하는 함수  (논블로킹모드 -> 반복적으로 )
void handle_pipe_read() {
    
    Message msg;
    int n;
    //파이프에서 메시지를 반복적으로 읽어처리 
    while(1){
        n=read(pipefd[0],&msg,sizeof(msg)); //파이프에서 메시지를 읽기.
        if(n>0){
            printf("[PARENT] Received message from child: [%s]: %s\n",msg.username, msg.content);
            broadcast_message(&msg,-1);
        }else if(n ==-1){
            //파이프에 읽은 데이터가 없을 경우, 루프를 빠젹나감.
            break;
        }
    }
}





int main(int argc, char **argv)
{
    //SIGUSR1 핸들러 설정
    signal(SIGCHLD,handle_sigchld);
    
    int ssock,csock;
    Message msg; //메시지 구조체 
    pid_t pid;
    struct sockaddr_in servaddr, cliaddr;/*주소 구조체 정의 */
    socklen_t clen=sizeof(cliaddr);  /*소켓 디스크립터 정의 */

    /*서버 소켓 생성*/
    if((ssock = socket(AF_INET, SOCK_STREAM,0))<0) {
        perror("socket()");
        return -1;
    }
        
    printf("Server Socket is created.! \n");

    //파이프 생성
    if (pipe(pipefd)==-1){
        perror("pipe()");
        return -1;
    }

    //논블로킹 모드로 설정

    int flags = fcntl(pipefd[0],F_GETFL,0);
    if(flags ==-1 || fcntl(pipefd[0],F_SETFL,flags | O_NONBLOCK)==-1){
        perror("fcntl()");
        return -1;
    }
    // 쓰기 끝도 논블로킹으로 설정 (필요할 경우)
    flags = fcntl(pipefd[1], F_GETFL, 0);
    if (flags == -1 || fcntl(pipefd[1], F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl()");
        return -1;
    }
    /*주소 구조체에 주소 지정 */
    memset(&servaddr, 0,sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);


    if(bind(ssock,(struct sockaddr *)&servaddr, sizeof(servaddr))<0) {
        perror("bind()");


        return -1;
    }
    /*대기하는 클라이언트 숫자 설정. */
    if(listen(ssock,MAX_CLIENTS)<0){
        perror("listen()");
        return -1;
    }

    printf("Server is listening on port %d\n",TCP_PORT);

    do {

        //이전에 남아있는 파이프 메시지 모두 처리
        handle_pipe_read();

        //클라이언트 연결 수락 
        csock = accept(ssock,(struct sockaddr *)&cliaddr, &clen);
        if(csock<0){

            perror("accept()");
            continue;
        }

        //자식프로세스 생성 
        if((pid=fork())<0){
            perror("Error");
        
        }else if (pid==0){
            close(ssock); //자식 프로세스에서는 서버 소켓을 닫음
            //로그인 및 메시지 처리
            do{
                memset(&msg, 0,sizeof(msg)); //메시지 구조체 초기화
                int n= recv(csock, &msg,sizeof(msg),0);
                
                //디버깅용 로그 추가
                //printf("Message received from client\n");

                printf(" content : %s\n",msg.content);


                if (n<=0){
                    if(n==0){
                        printf("Client disconnected.\n");
                    }else{
                        perror("recv()");
                    }
                    break;
                }
                if(strcmp(msg.content,"q")==0){
                    //클라이언트가 'q'
                    printf("%s has logout(q) the chat.\n",msg.username);
                    break;
                }

                if(strcmp(msg.type,"LOGIN")==0){
                    //클라이언트 로그인 처리
                    printf("[%s] logged in.\n",msg.username);
                    clients[client_count].sockfd = csock;
                    strcpy(clients[client_count].username, msg.username);
                    client_count++;
                } else if(strcmp(msg.type, "MSG")==0){
                    int bytes_written = write(pipefd[1],&msg,sizeof(msg));
                    if(bytes_written<=0){
                        perror("write()");
                    }
                    //else {
                    //    printf("Message written to pipe. Bytes: %d\n", bytes_written);
                    //}    
                    //서버가 받은 메시지를 클라이언트에게 다시 돌려보냄.
                    if(send(csock,&msg,sizeof(msg),0)<=0){
                        perror("send()");
                        break;
                    }
                }   
            }while(1); //종료 조건이 발생할 때까지 루프

            close(csock);/* 클라이언트 소켓을 닫음*/
            exit(0);     //자식 프로세스 종료
        }
        running_children++;  // 실행 중인 자식 프로세스 수 증가
        printf("New client connected. Current running children: %d\n", running_children);
        //printf("Parent process is checking the pipe for new message.\n");
        //handle_pipe_read();  // 부모프로세스는 파이프에서 메시지 읽기
        close(csock); //부모프로세스에서 클라이언트 소켓 닫기

    }while(1); //서버가 종료되기 전가지 클라이언트연결수락
    

    close(ssock);/*서버 소켓을 닫음 */
    return 0;
}
