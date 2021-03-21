/*
 NAME: Megan Pham
 EMAIL: megspham@ucla.edu
 ID: 505313300
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include <getopt.h>
#include <netinet/in.h> 
#include <math.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <mraa.h>
#include <mraa/aio.h>


mraa_aio_context tempSensor;
mraa_gpio_context button;

char scale = 'F';
FILE *logfd = 0;
int period = 1;
int logopt = 0;
int statusopt = 1; //0 for STOP, 1 for START

time_t start;
time_t end;
int id= 0;
int port =0; 
char *host="";
int sockfd=0;

SSL * ssl_client = NULL; 
SSL_CTX * context = NULL; 

const int B = 4275;    // B value of the thermistor
const int R0 = 100000; // R0 = 100k

SSL_CTX * ssl_init(void){
    SSL_CTX * newContext = NULL;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    newContext = SSL_CTX_new(TLSv1_client_method()); 
    return newContext; 
}

SSL * attach_ssl_to_socket (int sockfd, SSL_CTX * context){
    SSL *sslClient = SSL_new(context);
    SSL_set_fd(sslClient, sockfd);
    SSL_connect (sslClient);
    return sslClient;
}

void ssl_clean_client (SSL* client){
    SSL_shutdown(client);
    SSL_free(client);
}

int client_connect(char * host_name, unsigned int port){
    struct sockaddr_in serv_addr;
    int sockfd = socket (AF_INET, SOCK_STREAM, 0);
    struct hostent *server = gethostbyname(host_name);
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    memcpy (&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);

    serv_addr.sin_port = htons(port);
    connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr));
    return sockfd; 

}

float getTemperature()
{
    int a = mraa_aio_read(tempSensor);
    float R = 1023.0 / a - 1.0;
    R = R0 * R;
    float temperature = 1.0 / (log(R / R0) / B + 1 / 298.15) - 273.15; //right now it is in celsius
    if (scale == 'C')
    {
        return temperature;
    }
    //convert to farenheit
    return ((temperature * 9) / 5 + 32);
}

void getTime()
{
    time_t clock;
    int sec, min, hour;
    time(&clock);
    ctime(&clock);
    struct tm *local = localtime(&clock);

    sec = local->tm_sec;
    min = local->tm_min;
    hour = local->tm_hour;
    char str[256];
    sprintf(str, "%02d:%02d:%02d %.1f\n", hour, min, sec, getTemperature());
    if (logopt)
    {
        fprintf(logfd, "%s", str);
        fflush(logfd);
    }
    SSL_write(ssl_client, str, strlen(str));
}


void shut_down()
{
    time_t clock;
    int sec, min, hour;
    time(&clock);
    ctime(&clock);
    struct tm *local = localtime(&clock);

    sec = local->tm_sec;
    min = local->tm_min;
    hour = local->tm_hour;
    char temp[128];

    sprintf(temp, "%02d:%02d:%02d SHUTDOWN\n", hour, min, sec);
    if (logopt)
    {
        fprintf(logfd, "%s", temp);
        fflush(logfd);
        fclose(logfd);
    }
    SSL_write(ssl_client, temp, strlen(temp));

    ssl_clean_client(ssl_client);

    mraa_aio_close(tempSensor);
    mraa_gpio_close(button);
    exit(0);
}


void processCommands(char *input)
{
    if (strcmp(input, "SCALE=F") == 0)
    {
        scale = 'F';
        if (logopt)
            fprintf(logfd, "SCALE=F\n");       
    }
    else if (strcmp(input, "SCALE=C") == 0)
    {
        if (logopt)
            fprintf(logfd, "SCALE=C\n");
        scale = 'C';
    }
    else if (strcmp(input, "STOP") == 0)
    {
        if (logopt)
            fprintf(logfd, "STOP\n");
        statusopt = 0;
    }
    else if (strcmp(input, "START") == 0)
    {
        if (logopt)
            fprintf(logfd, "START\n");
        statusopt = 1;
    }
    else if (strncmp(input, "PERIOD=", 7) == 0)
    {
        if (logopt)
            fprintf(logfd, "%s\n", input);
        period = atoi(&input[7]);
    }
    else if (strncmp(input, "LOG", 3) == 0)
    {
        if (logopt)
            fprintf(logfd, "%s\n", input);
        else{
            char temp [20]= "no log file\n";
            SSL_write(ssl_client, temp, strlen(temp));
        }
    }
    else if (strcmp(input, "OFF") == 0)
    {
        if (logopt)
            fprintf(logfd, "OFF\n");
        shut_down();
    }
    else
    {
        fprintf(stderr, "invalid arguments\n");
        if (logopt)
            fprintf(logfd, "%s\n", input);
    }
}



struct option args[] = {
    {"period", 1, NULL, 'p'},
    {"scale", 1, NULL, 's'},
    {"log", 1, NULL, 'l'},
    {"id", 1, NULL, 'i'},
    {"host", 1, NULL, 'h'},
    {0, 0, 0, 0}};

int main(int argc, char **argv)
{
    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", args, NULL)) != -1)
    {
        switch (opt)
        {
        case 'p':
            period = atoi(optarg);
            break;
        case 's':
            if (optarg[0] != 'F' && optarg[0] != 'C')
            {
                fprintf(stderr, "Illegal argument for scale; can only be C or F\n");
                exit(1);
            }
            break;
        case 'l':
            logopt = 1;
            logfd = fopen(optarg, "a+");
            if (logfd == NULL)
            {
                fprintf(stderr, "cannot open file\n");
                exit(1);
            }
            break;
        case 'i':
            id = atoi(optarg);
            if (logopt)
                fprintf(logfd, "ID=%d\n", id);
            break;
        case 'h':
            host = optarg;
            break;
        default:
            fprintf(stderr, "Illegal argument, can only use --period=NUMBER, --scale=C/F, --log=NAME, --id=NUMBER, --host=NAME PORTNUMBER\n");
            exit(1);
        }
    }

    if (optind>=argc){
        fprintf(stderr, "Missing a port number\n");
    }

    port = atoi(argv[optind]);
    sockfd = client_connect(host, port);

    context = ssl_init();
    ssl_client = attach_ssl_to_socket(sockfd, context);
    char temp [100];

    sprintf (temp, "ID=%d\n", id);
    SSL_write(ssl_client, temp, strlen(temp));

    tempSensor = mraa_aio_init(1);
    button = mraa_gpio_init(60); //or use 62
    mraa_gpio_dir(button, MRAA_GPIO_IN);
    mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &shut_down, NULL);

    struct pollfd pollStdin = {sockfd, POLLIN, 0};

    int ret = 0;
    int r = 0; 
    int i = 0; //input index
    int l =0; //line index


    while (1)
    {
        time(&start);

        if (statusopt == 1 && difftime(start, end) >= (double)period)
        {
            getTime();
            time(&end);
        }

        ret = poll(&pollStdin, 1, 0);
        if (ret < 0)
        {
            fprintf(stderr, "polling error\n");
            exit(1);
        }

        if (pollStdin.revents & POLLIN){
            char line[256];
            char input[256];
            r = SSL_read(ssl_client, input, 256 * sizeof(char));
            //r = read(0, input, 256 * sizeof(char));
            if (r<0){
                fprintf(stderr, "reading error\n");
                exit(1);
            }

            for (i = 0; i < r; i++)
            {
                if (input[i] == '\n')
                {
                    processCommands(line);
                    memset(line, 0, 256 * sizeof(char)); //reset command line array
                    l = 0;
                }
                else
                {
                    line[l] = input[i];
                    l++;
                }
            }
        }

       
    }

    ssl_clean_client(ssl_client);

    mraa_aio_close(tempSensor);
    mraa_gpio_close(button);
    if (logopt)
    {
        fclose(logfd);
    }

    return (0);
}
