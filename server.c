#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include "server.h"
#include "str.h"
#include "const.h"

/*adds new element to list of sessions*/
static struct session *add_to_list(struct session *last, int fd)
{
	struct session *new;
	int i;
	new = malloc(sizeof(*new));
	new->buf_used = 0;
	new->next = NULL;
	new->fd = fd;
	new->name = NULL;
	new->state = login;
	new->money = st_money;
	new->material = st_material;
	new->product = st_product;
	new->to_prod = 0;
	new->prod_flag = 0;
	new->factories[0] = st_factory;
	for(i = 1; i < build_time+1; i++) {
		new->factories[i] = 0;
	}
	new->auction.buy_num = 0;
	new->auction.buy_price = 0;
	new->auction.sell_num = 0;
	new->auction.sell_price = 0;
	new->auction.buy_flag = 0;
	new->auction.sell_flag = 0;
	clear_buf(new->buf, buf_size);
	if(last != NULL) { 
		last->next = new;
	}
	last = new;
	return last;
}

/*deletes elements from list of sessions*/
void delete_from_list(struct session **pf, struct session **pl)
{
	struct session **pcur = pf;
	while(*pcur) {
		if((*pcur)->fd == -1) {
			struct session *t = *pcur;
			*pcur = (*pcur)->next;
			free(t);
		} else {
			/*new last element*/
			*pl = *pcur;
			pcur = &(*pcur)->next;
		}
	}
	if(*pf == NULL) {
		*pl = *pf;
	}
}

void close_session(struct session *del, int err)
{
	if(err) {
		int len = str_len(buf_overflow_err)-1;
		write(del->fd, buf_overflow_err, len);
	}
	shutdown(del->fd, SHUT_RDWR);
	close(del->fd);
	printf("Connection closed on fd: %d\n", del->fd);
	del->fd = -1;
}

/*reads port number from argv*/
static int read_port(const char *str)
{
	if(str == NULL) {
		printf("Error: port isn't specified\n");
		exit(1);
	}
	int num = 0;
	while(*str) {
		num *= 10;
		num += *str - '0';
		str++;
	}
	return num;
}

/*adds new client*/
static int new_client(int listen_soc)
{
	int fd;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	fd = accept(listen_soc, (struct sockaddr *)&addr, &len);
	write(fd, welcome_str, str_len(welcome_str)-1);
	printf("New client on fd: %d\n", fd);
	return fd;
}

/*changes option for testing*/
static void change_soc_opt(int listen_soc)
{
	int opt = 1;
	setsockopt(listen_soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

/*creates listen socket*/
int listen_soc_init(const char *port_str)
{
	int bind_res;
	int listen_soc;
	struct sockaddr_in addr;
	int port_num;
	port_num = read_port(port_str);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_num);
	addr.sin_addr.s_addr = htons(INADDR_ANY);
	listen_soc = socket(AF_INET, SOCK_STREAM, 0);
	change_soc_opt(listen_soc);
	/*gives addres to socket*/
	bind_res = bind(listen_soc, (struct sockaddr *)&addr, sizeof(addr));
	if(bind_res == -1) {
		perror(port_str);
	}
	listen(listen_soc, 32);
	return listen_soc;
}

/*analyzes client request*/
char *handle_request(struct session *cur)
{
	int len = -1;
	int i;
	char *request = NULL;
	/*tries to find '\n' in buffer*/
	for(i = 0; i < cur->buf_used; i++) {
		if(cur->buf[i] == '\n') {
			len = i;
			break;
		}
	}
	/*if no '\n'*/
	if(len == -1) {
		return request;
	}
	request = malloc((len+1)*sizeof(*request));
	copy_str(request, cur->buf, len);
	request[len] = 0;
	copy_str(cur->buf, cur->buf+len+1, buf_size-len-1);
	cur->buf_used -= (len+1);
	if(request[len-1] == '\r') {
	  	request[len-1] = 0;
	}
	return request;
}

/*reads symbols from client*/
int read_request(struct session *cur)
{
	int read_res;
	int b_used = cur->buf_used;
	read_res = read(cur->fd, cur->buf + b_used, buf_size - b_used);
	cur->buf_used += read_res;
	if(cur->buf_used >= buf_size) {
		close_session(cur, 1);
	}
	return read_res;
}

/*fills in used fds numbers in fd_set*/
void fill_fdset(const struct session *cur, fd_set *pread_fds, int *pmax_fd)
{
	while(cur) {
		int fd = cur->fd;
		FD_SET(fd, pread_fds);
		/*changes max number of used fds*/
		if(fd > *pmax_fd) {
			*pmax_fd = fd;
		}
		cur = cur->next;
	}
}

/*is there are new clients to accept*/
void accept_new_client(int ls, fd_set *pread_fds,
	struct session **pfirst, struct session **plast)
{
	if(FD_ISSET(ls, pread_fds)) {
		int fd = new_client(ls);
		*plast = add_to_list(*plast, fd);
		if(*pfirst == NULL) {
			*pfirst = *plast;
		}
	}
}

