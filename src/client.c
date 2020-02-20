#include "base.h"
#include "msg.h"

rfb_request *client_req = NULL;
rfb_display cli_display = {0};

static pthread_t pthread_tcp;

static void do_exit()
{
	void *tret = NULL;
	pthread_join(pthread_tcp, (void**)tret);  //等待线程同步
    DEBUG("pthread_exit client tcp");
}

static int send_pipe(char *buf, short cmd, int size)
{   
    int ret;
    set_request_head(buf, 0, cmd, size);
    ret = send_msg(pipe_cli[0], buf, size + HEAD_LEN);
    return ret;
}

static int send_done(rfb_request *req)
{
    int ret = ERROR;

	if(req->data_buf)
		free(req->data_buf);
	
	req->data_buf = malloc(sizeof(int) + 1);
	if(!req->data_buf)
		return ERROR;	
	
	req->data_size = sizeof(int);	

    *(int *)&req->data_buf[0] = 200;
    set_request_head(req->head_buf, 0, DONE_MSG_RET, sizeof(int));
    ret = send_request(req);
    
	DEBUG("done ok !!");
        
    req->status = OPTIONS;
    return ret;
}

static int recv_done(rfb_request *req)
{

  	if(read_msg_order(req->head_buf) == DONE_MSG)
    {   
        int ret;
        ret = send_pipe(req->head_buf, CLI_DONE_MSG, 0);
		req->status = OPTIONS;
		//ret= send_done(req);
		return ret;
    }
	return ERROR;
}

static int send_ready(rfb_request *req)
{   
    int ret; 
	if(req->data_buf)
		free(req->data_buf);
	
	req->data_buf = malloc(sizeof(int) + 1);
	if(!req->data_buf)
		return ERROR;	
	
	req->data_size = sizeof(int);	

    *(int *)&req->data_buf[0] = 200;
    set_request_head(req->head_buf, 0, READY_MSG, sizeof(int));
    ret = send_request(req);
	DEBUG("ready ok !!");

    req->status = DONE;
    return ret;
}

static int recv_ready(rfb_request *req)
{
	int ret = ERROR;
    if(read_msg_order(req->head_buf) == READY_MSG_RET)
    {   
        ret = send_pipe(req->head_buf, CLI_PLAY_MSG, 0);
        req->status = DONE;
        //ret = send_ready(req);
    }
    return ret;
}


static int send_options(rfb_request *req)
{
    int ret;
    if(req->data_buf)
        free(req->data_buf);

    req->data_buf = malloc(sizeof(int) + 1);

    if(!req->data_buf)
        return ERROR;

    *(int *)&req->data_buf[0] = 200;

    set_request_head(req->head_buf, 0, OPTIONS_MSG_RET, sizeof(int));
    req->data_size = sizeof(int);
    ret = send_request(req);

    if(SUCCESS != ret)
        return ERROR;

    req->status = READY;

	DEBUG("options ok !!");
    if(req->data_buf)
        free(req->data_buf);

    req->data_buf = NULL;

    return ret;
}

static int recv_options(rfb_request *req)
{
    int ret = ERROR;
    if(read_msg_order(req->head_buf) == OPTIONS_MSG_RET)
    {
        rfb_format *fmt = (rfb_format *)&req->data_buf[0];

        cli_display.play_flag = fmt->play_flag;
		if(fmt->play_flag == 2)
			req->control_udp = create_udp(server_ip, fmt->control_port);

		vids_width = fmt->width;
		vids_height = fmt->height;

        memcpy(&(cli_display.fmt), fmt, sizeof(rfb_format));
		cli_display.req = req;
        //ret = send_pipe(req->head_buf, CLI_UDP_MSG, 0);

        ret = send_options(req);

        DEBUG("fmt->width %d fmt->height %d fmt->code %d,"
          " fmt->data_port %d fmt->play_flag %d  fmt->bps %d fmt->fps%d",
             fmt->width, fmt->height,fmt->code, fmt->data_port,fmt->play_flag, fmt->bps, fmt->fps);
    }
    return ret;
}


static int recv_login(rfb_request *req)
{
    int ret = ERROR;
    if(read_msg_order(req->head_buf) == LOGIN_MSG_RET)
    {
        int server_major = 0, server_minor = 0;
        int client_major = 0, client_minor = 0;
        sscanf(req->data_buf, VERSIONFORMAT, &server_major, &server_minor);
        DEBUG(VERSIONFORMAT, server_major, server_minor);
        get_version(&client_major, &client_minor);
        if(server_major != client_major || server_minor != client_minor)
        {
            DEBUG("version not equel server: "VERSIONFORMAT" client: "VERSIONFORMAT"", server_major,
                    server_minor, client_major, client_minor);
            ret = ERROR;
        }
        if(req->data_buf)
            free(req->data_buf);
        req->data_buf = NULL;
        req->status = OPTIONS;
    }
    return ret;
}


static int send_login(rfb_request *req)
{
	int ret;
	int client_major = 0, client_minor = 0;

    DEBUG("client version :"VERSIONFORMAT, client_major, client_minor);
	get_version(&client_major, &client_minor);
    
    req->data_size = sz_verformat; 
    req->data_buf = (unsigned char *)malloc(req->data_size + 1);
    sprintf(req->data_buf, VERSIONFORMAT, client_major, client_minor);

    set_request_head(req->head_buf, 0,  LOGIN_MSG, SZ_VERFORMAT);
    ret = send_request(req);
    if(req->data_buf)
        free(req->data_buf);
    req->data_buf = NULL;
    
    req->status = LOGIN;
    return ret;
}

int process_client_pipe(char *msg, int len)
{   
    int ret = ERROR;
    switch(read_msg_order(msg))
    {   
        case CLI_PLAY_MSG:
        	ret = send_ready(client_req);
            break;
		case CLI_DONE_MSG:
			ret = send_done(client_req);
			break;
		defult:
			break;
    }
	return ret;
}

int process_client_msg(rfb_request *req)
{   
    int ret = 0;
    switch(req->status)
    {   
        case NORMAL:
        case LOGIN:
            ret = recv_login(req);
            break;
        case OPTIONS:
            ret = recv_options(req);
            break;
        case READY:
            ret = recv_ready(req);
            break;
        case DONE:
            ret = recv_done(req);
            break;
        case DEAD:
            break;
        default:
            break;
    }
    return ret;
}

void init_client()
{
	int ret = ERROR;
	int server_s = -1;

	init_X11();			

	get_screen_size(&screen_width, &screen_height);
	
	server_s = create_tcp();
	if(server_s == -1)
	{
	   	DEBUG("create tcp err");
        return;
	}	
    ret = connect_server(server_s, server_ip, server_port);
    if(0 != ret)
    {
        DEBUG("connect server ip: %s port %d error", server_ip, server_port);
        goto run_out;
    }

    client_req = new_request();
    if(!client_req)
    {
        DEBUG("malloc cli->head_buf sizeof: %d error :%s", HEAD_LEN, strerror(errno));
        goto run_out;
    }
	
	client_req->fd = server_s;

    ret = pthread_create(&pthread_tcp, NULL, thread_client_tcp, &server_s);
    if(0 != ret)
    {
        DEBUG("pthread create client tcp ret: %d error: %s",ret, strerror(ret));
        goto run_out;
    }

	ret = send_login(client_req);
    if(0 != ret)
    {
        DEBUG("send_login error");
        goto run_out;
    }
    do_exit();

run_out:
	close_fd(server_s);
	return;	
}

