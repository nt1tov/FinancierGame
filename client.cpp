#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h> //read, write, close


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> //socaddr_in structure, htonl func

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define BUFSIZE 1024

const char err_ip [] = "> Invalid IP adress recieved!\n";
const char err_msg[] = "> Error:\n";
const char err_cmdline[] = "> Use ./bot <join> <PlayerName> <IP> <port>\n";
const char err_connect[] = "> Cannot connect to server.Try again.\n";
//const char err_toolongbuf[] = ">Line too long!\n";


/*
class Client{
private:

public:

}

*/
struct metadata{
	
	char buf[BUFSIZE];
	int buf_used;
	char inbuf[BUFSIZE];
	int inbuf_used;

	
	int fd;
	long port;
	char *mode;
	const char *serv_ip;
	const char *serv_port;
	sockaddr_in addr;

};
int cmpstr(char *str1,  char *str2)
{
	int tmp = 1;
	while(*str1++ && *str2++){
		if(*str1 != *str2){
			tmp = 0;
			break;
		}
	}
	return tmp;
}

void reverse(char s[])
{
	int i, j;
	char c;
	for (i = 0, j = strlen(s)-1; i<j; i++, j--){
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}
void send_msg(int fd, const char *str)
{
	write(fd, str, strlen(str));
}


void launch(metadata *m)
{
	char *endptr;
	m->addr.sin_family = AF_INET;
	//translate port from char* to long
	m->addr.sin_port = htons(strtol(m->serv_port,  &endptr, 10));
	m->fd = socket(AF_INET, SOCK_STREAM, 0);

	//invalid socket create
	if(m->fd == -1){
		perror("socket");
		exit(1);
	}

	//invalid IP error
	if(!inet_aton(m->serv_ip, &(m->addr.sin_addr))){
		send_msg(1, err_msg);
		send_msg(1, err_ip);
	}

	//connect error
	if (0 != connect(m->fd, (sockaddr *)&(m->addr), sizeof(m->addr))){
		send_msg(1, err_msg);
		send_msg(1, err_connect);
	}


}
void server(metadata *m)
{
	int rc, bufp = m->buf_used;
	rc = read(m->fd, m->buf + bufp, BUFSIZE - bufp);
	if(rc <= 0){
		return;
	}
	m->buf_used += rc;

	//cycle for clean all read buffer
	for(;;){
		int pos = -1;
		int i;
		char *line;
		for(i = 0; i < m->buf_used; i++){
			if(m->buf[i] == '\n'){
				pos = i;
				break;
			}
		}
		if(pos == -1)
			return;
		//create server message for client
		line = new char[pos + 1];
		memcpy(line, m->buf, pos);
		line[pos] = '\0';
		if(line[pos-1] == '\r')
			line[pos-1] = '\0';
		printf(">server: %s\n", line);
		if(line)
        		delete[] line;
        	//clean server buffer
        	memmove(m->buf, m->buf+pos+1, BUFSIZE - pos - 1);
		m->buf_used -= (pos+1);
	}
}

void client(metadata *m)
{
	int bufp = m->inbuf_used;
	int rc = read(0, m->inbuf + bufp, BUFSIZE - bufp);
	if(rc <= 0){
		return;
	}
	m->inbuf_used += rc;
	int pos = -1;
	int i;
	char *line;
	//search message separators
	for(i = 0; i < m->inbuf_used; i++){
		if(m->inbuf[i] == '\n'){
			pos = i;
			break;
		}
	}
	if(pos == -1)
		return;
	line = new char[pos + 2];
	//create message for server from client
	memcpy(line, m->inbuf, pos+1);
	line[pos+1] = '\0';
	if(line[pos-1] == '\r'){
		line[pos-1] = '\n';
		line[pos] = '\0';
	}
	write(m->fd, line, strlen(line));
	if(line)
		delete[] line;
	//clean client buffer
	memmove(m->inbuf, m->inbuf+pos+1, BUFSIZE - pos - 1);
	m->inbuf_used -= (pos+1);
}

int main(int argc, char **argv)
{
	metadata *meta = new metadata;

	if(argc != 2)
		send_msg(1, err_cmdline);

	meta->serv_port = argv[1];
	meta->serv_ip = "0.0.0.0";

	launch(meta);
	
	fd_set readfds;
	int maxfd = meta->fd;
	for(;;){
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(meta->fd, &readfds);
		int sr = select(maxfd+1, &readfds, NULL, NULL, NULL);
       		if(sr == -1) {
            		perror("select");
            		return 1;
        	}
        	if(FD_ISSET(meta->fd, &readfds)){
        		server(meta);
        	}

        	if(FD_ISSET(0, &readfds)){
        		client(meta);
        	}
	}
	return 0;
}