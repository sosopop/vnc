#include "base.h"

/* config.ini */
int server_flag = 0;

int client_port = -1, control_port = -1, h264_port = -1, server_port = -1, window_flag = 0, window_size = 0;
int default_quality = 0, default_fps = 0;
int max_connections = -1; 
char *server_ip = NULL;
int run_flag = 0;
int status = NORMAL;
time_t last_time;
time_t current_time;


#if 1
void parse_options()
{  
	/* arg > file */
    int ret = 0;
    char buf[126] = {0};
	
    ret = read_profile_string(BASE_SECTION, BASE_TYPE_KEY, buf, sizeof(buf), DEFAULT_TYPE, CONFIG_FILE);
    if(ret && !strncmp(buf, "server", 6))
    {   
        server_flag = 1;
    }
    /* 读取对应所需要的配置信息 */
    if(server_flag)     //服务端程序
    {   
        client_port = read_profile_int(SERVER_SECTION, SERVER_CLIENT_PORT_KEY, DEFAULT_CLIENT_PORT_VALUE, CONFIG_FILE);
        control_port = read_profile_int(SERVER_SECTION, SERVER_CONTROL_PORT_KEY, DEFAULT_CONTROL_PORT_VALUE, CONFIG_FILE);
        h264_port = read_profile_int(SERVER_SECTION, SERVER_H264_PORT_KEY, DEFAULT_H264_PORT_VALUE, CONFIG_FILE);
        window_flag = read_profile_int(SERVER_SECTION, SERVER_WINDOW_FLAG_KEY, DEFAULT_WINDOW_FLAG_VALUE, CONFIG_FILE);
        window_size = read_profile_int(SERVER_SECTION, SERVER_WINDOW_SIZE_KEY, DEFAULT_WINDOW_SIZE_VALUE, CONFIG_FILE);
        DEBUG("\nprograme server: \n client_port %d, control_port %d, h264_port %d, window_flag %d, window_size %d,",
                 client_port, control_port, h264_port, window_flag, window_size);
    }
    else                //客户端程序
    {   
        ret = read_profile_string(CLIENT_SECTION, CLIENT_IP_KEY, buf, sizeof(buf), DEFAULT_IP_VALUE, CONFIG_FILE);
		server_ip = strdup(buf);	
	
        server_port = read_profile_int(CLIENT_SECTION, CLIENT_PORT_KEY, DEFAULT_PORT_VALUE, CONFIG_FILE);
        default_quality = read_profile_int(CLIENT_SECTION, CLIENT_QUALITY_KEY, DEFAULT_QUALITY_VALUE, CONFIG_FILE);
        default_fps = read_profile_int(CLIENT_SECTION, CLIENT_FPS_KEY, DEFAULT_FPS_VALUE, CONFIG_FILE);
        DEBUG("\nprograme client: \n server_ip %s, server_port %d, window_flag %d, default_quality %d default_fps %d",
                server_ip, server_port, window_flag, default_quality, default_fps);
    }
}
#else

void parse_options(int argc, char *argv[])
{
    switch(argc)
    {   
        case 3:
            server_port = atoi(argv[2]);
        case 2:
            server_ip = strdup(argv[1]); 
        default:
            break;
    }   

    if(!server_ip || server_port >= 65535 || server_port <= 0)
    {   
    	int ret = 0;
    	char buf[126] = {0};
   		ret = read_profile_string(CLIENT_SECTION, CLIENT_IP_KEY, buf, sizeof(buf), DEFAULT_IP_VALUE, CONFIG_FILE);
		server_ip = strdup(buf);	
        server_port = read_profile_int(CLIENT_SECTION, CLIENT_PORT_KEY, DEFAULT_PORT_VALUE, CONFIG_FILE);
    }   

}
#endif

void sig_quit_listen(int e)
{
    char s = 'S';

    //write( pipeid[1], &s,  sizeof(s));
    DEBUG("recv stop msg");
    return;
}

void do_exit()
{

    if(server_flag)
    {
        exit_server();
    }
    else
    {
        exit_client();
	}

}


#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
	(void) time(&current_time);	

	run_flag = 1;

    init_logs();

    /* config */
    //parse_options(argc, argv);
    parse_options();
	
    if(server_flag)
    {
        init_server();
    }
    else
    {
        init_client();
    }
	
	do_exit();
	close_logs();
	

	return 0;
}


