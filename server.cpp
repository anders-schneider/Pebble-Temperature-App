/* 
Authors: Anders Schneider, Josh Taylor, Alifia Haidry
Code for communicating with the server.

Parts of this code comes from
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <queue>
#include <cmath>
#define QUEUE_SIZE 3600
using namespace std;


pthread_mutex_t lock;
pthread_mutex_t buflock;
pthread_mutex_t queuelock;
pthread_mutex_t statslock;
int flag = 0;
char globbuf[1000];
queue<double> t;
double total;
double minimum = 1000000;
double maximum = -1000000;
int sendMessage = -1;
int temp_counter = 0;
int time_counter = 0;
bool connectedToArduino = false;

/*This method configures the server connection, waits for the response,
gets request from the watch, generates a reply in the form of a json, key-value pair 
and sends this reply over the socket. It then closes the socket. 
*/
void * start_server(void* port_arg)

{
      int PORT_NUMBER = *((int*) port_arg);

      // structs to represent the server and client
      struct sockaddr_in server_addr,client_addr;    
      
      int sock; // socket descriptor
      int fd; //file descriptor

      // 1. socket: creates a socket descriptor that you later use to make other system calls
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
				perror("Socket");
				exit(1);
      }
      int temp;
      if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
				perror("Setsockopt");
				exit(1);
      }

      // configure the server
      server_addr.sin_port = htons(PORT_NUMBER); // specify port number
      server_addr.sin_family = AF_INET;         
      server_addr.sin_addr.s_addr = INADDR_ANY; 
      bzero(&(server_addr.sin_zero),8); 
      
      // 2. bind: use the socket and associate it with the port number
      if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
				perror("Unable to bind");
				exit(1);
      }

      // 3. listen: indicates that we want to listen to the port to which we bound; second arg is number of allowed connections
      if (listen(sock, 5) == -1) {
				perror("Listen");
				exit(1);
      }
          
      // once you get here, the server is set up and about to start listening
      printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
      fflush(stdout);
     

      // 4. accept: wait here until we get a connection on that port
      int sin_size; 
      while(1){
       	pthread_mutex_lock(&lock);
       	if(flag == 1){
         
          pthread_mutex_unlock(&lock);
          break;
        }
       	pthread_mutex_unlock(&lock);

        sin_size  = sizeof(struct sockaddr_in);
        printf("waiting for connection\n");
	      fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);

	      printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
	      
	      // buffer to read data into
	      char request[1024];
	      
	      // 5. recv: read incoming message into buffer
	      int bytes_received = recv(fd,request,1024,0);

	      // null-terminating the string
	      request[bytes_received] = '\0';
	      char c = request[5];
	      int a = c - '0';
 
	      // this is the message that we'll send back
	      /* it actually looks like this:
	        {
	           "name": "cit595"
	        }
	      */
	      char reply[150];

	      //Contructs error response if arduino is not connected.
	      if(!connectedToArduino){
	        strcpy(reply,"{\n\"not_connected_to_arduino\": \"not_connected_to_arduino");
	      } 

	      //Constructs the appropriate response for current temperature when the key value is 0
	      else if(a == 0){
	        strcpy(reply,"{\n\"cur_temp\": \"");
	        pthread_mutex_lock(&buflock);
	        strcat(reply,globbuf);
	        pthread_mutex_unlock(&buflock);
	       }

	       //Constructs the appropriate response for statistics when the key value is 1
	       else if(a == 1){
	         pthread_mutex_lock(&queuelock);
	        if(temp_counter > 0){
	          strcpy(reply,"{\n\"stats\": \"");
	          char b[150];
	          sprintf(b,"%lf,%lf,%lf,%d",minimum, maximum, total/temp_counter, time_counter);
	          strcat(reply,b);
	        }
	        else{
	          strcpy(reply,"{\n\"no_stats_data\": \"no_stats");
	        }
	        pthread_mutex_unlock(&queuelock);
	          
	       }

	       //Constructs the appropriate response for standby mode when the key value is 2
	       else if(a == 2){
	          strcpy(reply,"{\n\"standby\": \"");
	          sendMessage = a;
	       }

	       //Constructs the appropriate response for wake up mode when the key value is 3
	       else if(a == 3){
	          strcpy(reply,"{\n\"wake_up\": \"");
	          sendMessage = a;
	       }

	       //Constructs the appropriate response for changing unit to Celsius when the key value is 4
	       else if(a == 4){
	          strcpy(reply,"{\n\"Celsius\": \"");
	          sendMessage = a;
	       }

	       //Constructs the appropriate  response for changing unit to Farenheit when the key value is 5
	       else if(a == 5){
	          strcpy(reply,"{\n\"Farenheit\": \"");
	          sendMessage = a;
	       }

	       //Constructs the appropriate response for turning on dance mode when the key value is 6
	       else if(a == 6){
	          strcpy(reply,"{\n\"dance_on\": \"dance_on");
	          sendMessage = a;
	       }

	       //Constructs the appropriate response turning off dance mode when the key value is 7
	       else if(a == 7){
	          strcpy(reply,"{\n\"dance_off\": \"");
	          sendMessage = a;
	       }

	       //Constructs error response otherwise
	       else {
	          strcpy(reply,"{\n\"error\": \"");
	       }
	       
	       strcat(reply,"\"\n}\n");
	      
	      // 6. send: send the message over the socket
	      send(fd, reply, strlen(reply), 0);

	      // close the file descriptor.
	      close(fd);
      }
      
      // 7. close: close the socket connection
      close(sock);
      printf("Server closed connection\n");
  
      return 0;
} 

/*This method reads the user input from the console
and quits the program when user enter 'q'
*/
void* input(void* r){
  char input[256];
  
  while(1){
    bzero(input, 256);
    printf("Enter Input.\n");
    scanf("%s", input);
    if (!strcmp(input, "q")) {
      pthread_mutex_lock(&lock);
      flag = 1;
      pthread_mutex_unlock(&lock);
      break;  //quit on "q"  
    }
  }
 
  pthread_exit(NULL);
}

/* Method for comparing doubles
*/
bool isEqual(double num1, double num2){
    return abs(num1 - num2) < 0.01;
}

/* This method searches the minimum of all the elements in a queue. 
It traverses the whole queue by poping , comparing to the minimum value 
and pushing back to the queue, resulting in the queue order being intact.
*/
void findNewMin(){
  int counter = 0;
  double locmin = 1000000;
  while(counter < QUEUE_SIZE){
    double num = t.front();
    t.pop();
    if(num < locmin){
      locmin = num;
    }
    t.push(num);
    counter++;
  }
  minimum = locmin;
}

/* This method searches the maximum of all the elements in a queue. 
It traverses the whole queue by poping , comparing to the maximum value 
and pushing back to the queue, resulting in the queue order being intact.
*/
void findNewMax(){
  int counter = 0;
  double locmax = -1000000;
  while(counter < QUEUE_SIZE){
    double num = t.front();
    t.pop();
    if(num > locmax && num < 9000.0){
      locmax = num;
    }
    t.push(num);
    counter++;
  }
  maximum = locmax;
}


/* This method configures the serial connection, reads in data from the arduino 
and stores it into local buffer, it then populates the global queue with the temperature 
readings coming from the arduino and updates the minimum, maximum and total temperatures
for every incoming reading which are stored as globals. It also sends message to the arduino 
for sleep and wake up modes.When the user enter 'q', it closes the serial connection and 
returns from the method.
*/
void* serial(void* p){
  char buf[100];
  int fd = open("/dev/cu.usbmodem1411", O_RDWR);

  // error checking, if open returns -1, something went wrong!
  if(fd == -1){
    printf("Could not open connection.");
    connectedToArduino = false;
    return 0;
  }
  connectedToArduino = true;

  // configuring connection
  struct termios options;
  tcgetattr(fd, &options);
  cfsetispeed(&options, 9600);
  cfsetospeed(&options, 9600);
  tcsetattr(fd, TCSANOW, &options);

  
  bzero(buf,100);
  int bytes_read =0;
  char locbuf[1000];
  bzero(locbuf,1000);
  bool standby = false;
  while(1){
    if(sendMessage != -1){
      write(fd, &sendMessage, 1);
      if(sendMessage == 2){
        standby = true;
      }
      else if(sendMessage == 3){
        standby = false;
      }
       sendMessage = -1;
    }

    bzero(buf,100);

    //reading data from arduino
    bytes_read = read(fd, buf, 100);
    if(bytes_read == -1){
      connectedToArduino = false;
    }
    if(bytes_read != 0) {
      strcat(locbuf,buf);
      if(buf[bytes_read - 1] == '\n'){
        pthread_mutex_lock(&buflock);
        bzero(globbuf,1000);
        strncpy(globbuf,locbuf,strlen(locbuf) - 1);
        double temp = stod(globbuf);
        pthread_mutex_unlock(&buflock);
        pthread_mutex_lock(&queuelock);
        t.push(temp);

        // updates the minimum, maximum and total values
        // only when the data received is not junk data.
        if(temp < 9000.0){
            if(temp < minimum){
             minimum = temp;
            }
            if(temp > maximum){
              maximum = temp;
            }
            total = total + temp;
            temp_counter++;
            time_counter++;
        }  
        
        //updates the minimum, maximum and total values when queue size 
        //exceeds the predefined QUEUE_SIZE
        if(t.size() > QUEUE_SIZE){
          double lost = t.front();
          t.pop();

          // updates the minimum, maximum and total values
          // only when the data received is not junk data.
          if(lost < 9000.0){
            total = total - lost;
            temp_counter--;
            if(isEqual(lost,minimum)){
              findNewMin();
            }
            if(isEqual(lost,maximum)){
             findNewMax();
            }
          }   
          
        }

        pthread_mutex_unlock(&queuelock);

        bzero(locbuf,1000);
      }
      

    }
      pthread_mutex_lock(&lock);

      //quitting the method if the user enters 'q'
      if(flag == 1){
        pthread_mutex_unlock(&lock);
        break;
      }
      pthread_mutex_unlock(&lock);
    
  }

  // when you're done
  close(fd);
  return 0;
}

/*Main Method*/
int main(int argc, char *argv[])
{
  // check the number of arguments
  if (argc != 2)
    {
      printf("\nUsage: server [port_number]\n");
      exit(0);
    }

  int PORT_NUMBER = atoi(argv[1]);
  pthread_t t1, t2, t3;
  pthread_mutex_init(&lock, NULL);

  //Creating threads t1, t2 and t3.
  pthread_create(&t1,NULL,&start_server,&PORT_NUMBER);
  pthread_create(&t2,NULL,&input,NULL);
  pthread_create(&t3,NULL,&serial,NULL);

  //Joining threads t1, t2 and t3.
  pthread_join(t3,NULL);
  pthread_join(t2,NULL);
  pthread_join(t1,NULL);
}
