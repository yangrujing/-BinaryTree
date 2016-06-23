#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<sys/sendfile.h>
#include<sys/wait.h>
#include<unistd.h>
#include<string.h>
#include<strings.h>
#include<errno.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<pthread.h>
#include<ctype.h>
#include<assert.h>
#include<fcntl.h>
#include<termios.h>

#define _BACK_LOG_ 5
#define _BLOCK_SIZE_ 2048

//create socket
int startup()
{
    int listen_sock = socket(AF_INET,SOCK_STREAM,0);
    if(listen_sock < 0)
    {
        perror("socket");
        exit(1);
    }

    int flag = 1;
    setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag)); //Avoid port reuse
    
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(8080);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
                                                                        
                                                                        
    if(bind(listen_sock,(struct sockaddr*)&local,sizeof(local)) < 0)                            
    {
        perror("bind");
        exit(2);
    }
    
    
    if(listen(listen_sock,_BACK_LOG_) < 0)
    {
        perror("listen");
        exit(3);
    }                                                                                       
    return listen_sock;
}

int read_data(int sock,char *buf,int len)
{
    printf("read_data\n");
    ssize_t size = -1;
    int total = 0;
    while(1)
    {
        //ssize_t recv(int sockfd, void *buf, size_t len, int flags);
        size = recv(sock,buf+total,len -1,MSG_DONTWAIT);
        if(size > 0)
        {
           total += size;
        }
        else if(size == 0)
            return 0;
        else
            return -1;
    }
}

//串口设置
int  SerialPortOPT()
{
    int fd = open("/dev/ttyS3",O_RDWR);////打开串口
    if(fd == -1)
    {
        perror("open serial port");
        return -1;
    }
#ifdef _DEBUG_

    struct termios newtio,oldtio;  
    if  ( tcgetattr( fd,&oldtio)  !=  0) 
    {   
        perror("SetupSerial 1");  
        return -1;  
    }  
    bzero( &newtio, sizeof( newtio ) ); 
    newtio.c_cflag  |=  CLOCAL | CREAD; 
    newtio.c_cflag &= ~CSIZE; 
    //设置数据位
    newtio.c_cflag |= CS8;  
    //设置波特率为9600
    cfsetispeed(&newtio, B9600);  
    cfsetospeed(&newtio, B9600); 

#endif
    return fd;
}
//$GPGGA, <1>,<2>,<3>,<4>,<5>,<6>,<7>,<8>,<9>,M,<10>,M,<11><12>*hh<CR><LF>
//eg:$GPGGA,161229.478,3723.2475,N,12158.3416,W,1,07,1,1.0,9.0,M,7.3,M,,0000,*18
//<1>   UTC时间，时时分分秒秒格式。 
//<2>   纬度，度度分分.分分分分格式(第 1 位是0也将传送)。 
//<3>   纬度半球，N或 S(北纬或南纬)。 
//<4>   经度，度度度分分.分分分分格式(第 1 位0也将传送)。 
//<5>   经度半球，E 或 W(东经或西经)。 
//<6>   GPS质量指示，0：方位无法使用，1=非差分 GPS 获得方位，2=差分方式获得方位(DGPS)，6=估计获得。 
//<7>   使用卫星数量，从 00 到12(第1位是 0 也将传送)。 
//<8>   水平精确度，0.5～99.9。 
//<9>   天线离海平面的高度，-9 999.9～9999.9m。M 指单位米。 
//<10>  大地水准面高度，-999.9～9999.9m。M 指单位米。 
//<11>  差分GPS数据期限(RTCM SC-104)，最后设立RTCM传送的秒数量(例如无DGPS为0)。 
//<12>  差分参考基站标号，从 0000～1023,实时DGPS 时无。 
//*     语句结束标志符。 
//hh    从$开始的所有 ASCII码校验和。 
//<CR> <LF>回车换行。

float Time,latitude/*纬度*/,longitude/*经度*/,LevelAccuracy/*水平精确*/,AntennaHeight/*天线高度*/,altitude/*海拔高度*/;
char LatHem/*纬度半球*/,LongHem/*经度半球*/,Unit1,Unit2;
int GPSvalid,SatelliteNum/*卫星数量*/;

void AnalyzeData(char *buf,int size)
{
    sscanf(buf,"$GPGGA,%f,%f,%c,%f,%c,%d,%02d,%f,%f,%c,%f,%c", &Time, &latitude, &LatHem, &longitude, &LongHem, &GPSvalid, &SatelliteNum, \
    &LevelAccuracy, &AntennaHeight, &Unit1, &altitude,&Unit2);
    printf("纬度: %c %f, 经度: %c %f, 海拔高度: %f, 卫星数量: %d\n",LatHem, latitude, LongHem, longitude, altitude, SatelliteNum);
}

#ifdef _DEBUG_

void DataBaseOPT()
{
    char data[_BLOCK_SIZE_];
    memset(data,'\0',sizeof(data));
    sprintf(data,"'%f','%c','%f','%c','%f'",latitude,LatHem,longitude,LongHem,altitude);
    printf("data : %s",data);
    sql_connect _sql;
    _sql.connect_mysql();
    _sql.insert_mysql(data);
}

#endif

void echo_html(int sock,char *path)
{
    struct stat st; 
    //int stat(const char *path, struct stat *buf); 
    // 通过文件名获取文件信息，并保存在buf所指的结构体stat中
    if(stat(path,&st) < 0) //failed file not exist
    {
        perror("stat");
        close(sock);
    }
    //打开地图
    int fd = open(path,O_RDONLY);
    if(fd < 0)
    {
        perror("open map");
        return;
    }

    char state_line[_BLOCK_SIZE_ / 2];
    memset(state_line,'\0',sizeof(state_line));
    strncpy(state_line,"HTTP/1.1",strlen("HTTP?1.1") + 1);
    strcat(state_line,"200 OK");
    strcat(state_line,"\r\n\r\n");
    if(send(sock,state_line,strlen(state_line),0) < 0)
    {
        perror("send");
        return;
    }
    //ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
    if(sendfile(sock,fd,NULL,st.st_size) < 0)
    {
         perror("sendfile");
         close(fd);
         return;
    }
    close(fd);
}

int main()
{
    char buf[_BLOCK_SIZE_];
    memset(buf,'\0',sizeof(buf));

    struct sockaddr_in client;
    socklen_t len  = sizeof(client);

    int sock = startup();
    while(1)
    {
        int new_client = accept(sock,(struct sockaddr*)&client,&len);
        if(new_client < 0)
        {
            perror("accept");
            continue;
        }
        printf("Get a connect\n");

        if(read_data(new_client,buf,sizeof(buf)) == 0)
        {
            printf("close connect..\n");
                                
        }
        printf("%s\n",buf);                                                      
        memset(buf,'\0',sizeof(buf));
        sprintf(buf,"HTTP/1.0 200 OK\r\n\r\n");
        if(send(new_client,buf,strlen(buf),0) == -1)
        {   
            perror("send"); 
            exit(1); 
        }
        
        //打开并设置串口
       int fd =  SerialPortOPT();
       ssize_t size = read(fd,buf,sizeof(buf) - 1);
       if(size < 0)
       {
            perror("read data");
            close(fd);
            return -1;
       }
       if(strstr(buf,"$GPGGA") != NULL)
       {
            AnalyzeData(buf,sizeof(buf));
       }
    //DataBaseOPT();
    
    //map path
    char *path = "htdocs/map.html";
    echo_html(new_client,path);

    close(fd);
    }
    close(sock);
    return 0;
}
