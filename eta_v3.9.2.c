// eta-mqtt-bridge v3.9.2
// ETA RS232 → MQTT (Einzel-Topics + RAW + Availability + factor-cmd)
// Build: gcc -O2 -Wall -pthread -o eta_v3.9.2 eta_v3.9.2.c -lmosquitto
// Runtime: ./eta_v3.9.2 /dev/ttyUSB0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mosquitto.h>

static int tty_fd=-1;
static pthread_t thr_read,thr_pub;
static volatile int running=1;
static pthread_mutex_t data_mx=PTHREAD_MUTEX_INITIALIZER;
static struct mosquitto *mq=NULL;

// ---- ETA Registerdefinition ----
static unsigned char datas[10][3]={
 {0x08,0x00,0x0c},{0x08,0x00,0x0b},{0x08,0x00,0x0a},
 {0x08,0x00,0x0d},{0x08,0x00,0x46},{0x08,0x00,0x4b},
 {0x08,0x00,0x15},{0x08,0x00,0x08},{0x08,0x00,0x09},{0x08,0x00,0x44}};
static const char *keys[10]={
 "puffer_oben","puffer_mitte","puffer_unten","boiler","aussen",
 "puffer_ladezustand","abgas","kessel","ruecklauf","vorlauf_mk1"};
static float werte[10]; static int rawv[10];
// Default-Scaling: temp=raw*0.1, % = raw*1.0, abgas kalibriert
static double factor[10]={0.1,0.1,0.1,0.1,0.1,1.0,0.5373,0.1,0.1,0.1};

// sauber abmelden
static const unsigned char unsub[6]={'{','M','E',0,0,'}'};
static const unsigned char start[3]={'{','M','C'};

// ---- MQTT Basisparameter ----
// ⚠️ ANPASSEN: Setze hier deine echten Werte oder nutze env-Datei/Installer
static char mqtt_host[64]="MQTT_SERVER_IP";     // z.B. 192.168.1.10  (⚠️ ANPASSEN)
static int  mqtt_port=1883;                     // z.B. 1883          (optional anpassen)
static char mqtt_user[64]="MQTT_USER";          // z.B. ha_mqtt       (⚠️ ANPASSEN)
static char mqtt_pass[64]="MQTT_PASSWORD";      // z.B. geheim        (⚠️ ANPASSEN)

static const char *topic_base="heizung/status";         // Topics/Werte
static const char *avail="heizung/status/availability"; // Availability
static const char *cmd_topic="heizung/cmd";             // Befehle (set_factor …)
static int interval=30;                                  // Publish-Intervall (sek)

// -------------------------------
static void sig(int s){(void)s;running=0;}

static int tty_open(const char *dev){
 struct termios t;
 tty_fd=open(dev,O_RDWR|O_NOCTTY|O_NONBLOCK);
 if(tty_fd<0){perror("tty open");return -1;}
 tcgetattr(tty_fd,&t);
 cfmakeraw(&t);
 cfsetispeed(&t,B19200);cfsetospeed(&t,B19200);
 t.c_cflag|=(CLOCAL|CREAD|CS8);
 t.c_cflag&=~(PARENB|CSTOPB);
 tcsetattr(tty_fd,TCSANOW,&t);
 return tty_fd;
}

static void send_sub(int s){
 unsigned char b[128];int l=0,chk=0;
 memcpy(b+0,start,3);l=6;b[5]=s;
 for(int i=0;i<10;i++){memcpy(b+l,datas[i],3);l+=3;}
 for(int i=5;i<l;i++)chk+=b[i];
 b[3]=l-5;b[4]=chk&0xFF;b[l++]='}';
 write(tty_fd,b,l);
}

static void *reader(void *a){
 (void)a;unsigned char ch;unsigned int f[512];
 int fl=0,nl=-1;send_sub(20);
 while(running){
  int r=read(tty_fd,&ch,1);
  if(r<=0){usleep(20000);continue;}
  if(fl==3)nl=ch;f[fl++]=ch;
  if(ch=='}'&&nl>0&&fl>=nl){
   pthread_mutex_lock(&data_mx);
   for(int i=0;i<10;i++)
    for(int a=5;a<fl;a+=5)
     if(f[a]==datas[i][0]&&f[a+1]==datas[i][1]&&f[a+2]==datas[i][2]){
      float raw=f[a+3]*256+f[a+4];
      if(raw>65000)raw-=65536;
      if(raw<-1000||raw>10000)raw=0;
      rawv[i]=raw;werte[i]=raw*factor[i];
     }
   pthread_mutex_unlock(&data_mx);
   send_sub(20);fl=0;nl=-1;
  }
 }return NULL;
}

// ---- MQTT ----
static void on_connect(struct mosquitto*m,void*u,int rc){
 (void)u;
 if(!rc){
  fprintf(stderr,"[MQTT] connected %s:%d\n", mqtt_host, mqtt_port);
  mosquitto_publish(m,NULL,avail,6,"online",1,true);      // Availability → online
  mosquitto_subscribe(m,NULL,cmd_topic,1);                // Befehle empfangen
 }else fprintf(stderr,"[MQTT] connect error %d\n",rc);
}

static void on_msg(struct mosquitto*m,void*u,const struct mosquitto_message*msg){
 (void)m;(void)u;
 if(!msg||!msg->topic||!msg->payload)return;
 if(strcmp(msg->topic,cmd_topic)==0){
  const char*p=(const char*)msg->payload;
  if(strncmp(p,"set_factor ",11)==0){
   p+=11;
   for(int i=0;i<10;i++){
    char k[64];snprintf(k,sizeof(k),"%s=",keys[i]);
    const char*f=strstr(p,k);
    if(f){
     double v=atof(f+strlen(k));
     if(v>0.0&&v<100.0){
       pthread_mutex_lock(&data_mx);
       factor[i]=v;
       pthread_mutex_unlock(&data_mx);
       fprintf(stderr,"[CMD] factor %s=%.4f\n",keys[i],v);
     }
    }
   }
  }
 }
}

static int mqtt_init(void){
 mosquitto_lib_init();
 mq=mosquitto_new(NULL,true,NULL);
 if(!mq){fprintf(stderr,"mosquitto_new fail\n");return-1;}

 // Last Will → offline (retained)
 mosquitto_will_set(mq,avail,7,"offline",1,true);

 if(strlen(mqtt_user)) mosquitto_username_pw_set(mq,mqtt_user,strlen(mqtt_pass)?mqtt_pass:NULL);
 mosquitto_connect_callback_set(mq,on_connect);
 mosquitto_message_callback_set(mq,on_msg);

 int rc=mosquitto_connect_async(mq,mqtt_host,mqtt_port,30);
 if(rc){fprintf(stderr,"MQTT connect %s\n",mosquitto_strerror(rc));return-1;}
 rc=mosquitto_loop_start(mq);
 if(rc){fprintf(stderr,"MQTT loop_start %s\n",mosquitto_strerror(rc));return-1;}
 return 0;
}

static void *publisher(void*a){
 (void)a;
 while(running){
  pthread_mutex_lock(&data_mx);
  for(int i=0;i<10;i++){
   char t[128],p[64];snprintf(t,sizeof(t),"%s/%s",topic_base,keys[i]);
   snprintf(p,sizeof(p),"%.2f",werte[i]);
   int rc=mosquitto_publish(mq,NULL,t,(int)strlen(p),p,1,true);
   if(rc)fprintf(stderr,"[MQTT] publish FAIL %s : %s\n",t,mosquitto_strerror(rc));

   snprintf(t,sizeof(t),"%s/%s_raw",topic_base,keys[i]);
   snprintf(p,sizeof(p),"%d",rawv[i]);
   rc=mosquitto_publish(mq,NULL,t,(int)strlen(p),p,0,true);
   if(rc)fprintf(stderr,"[MQTT] publish FAIL %s : %s\n",t,mosquitto_strerror(rc));
  }
  pthread_mutex_unlock(&data_mx);
  for(int i=0;i<interval&&running;i++) sleep(1);
 }return NULL;
}

int main(int argc,char*argv[]){
 if(argc<2){fprintf(stderr,"Usage: %s /dev/ttyUSB0\n",argv[0]);return 1;}
 signal(SIGINT,sig);signal(SIGTERM,sig);
 if(tty_open(argv[1])<0)return 2;
 if(mqtt_init()<0)return 3;
 pthread_create(&thr_read,NULL,reader,NULL);
 pthread_create(&thr_pub,NULL,publisher,NULL);
 pthread_join(thr_read,NULL);
 pthread_join(thr_pub,NULL);
 if(tty_fd>=0)write(tty_fd,unsub,sizeof(unsub));
 mosquitto_loop_stop(mq,true);mosquitto_disconnect(mq);
 mosquitto_destroy(mq);mosquitto_lib_cleanup(); close(tty_fd);
 return 0;
}
