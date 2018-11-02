#include <syslog.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#define MB 1024*1024

#include <stdio.h>
#include <stddef.h>

/* a ub4 is an unsigned 4-byte quantity */
typedef  unsigned long int  ub4;

/* external results */
ub4 randrsl[256], randcnt;

/* internal state */
static    ub4 mm[256];
static    ub4 aa = 0, bb = 0, cc = 0;


void isaac()
{
	register ub4 i, x, y;

	cc = cc + 1;    /* cc just gets incremented once per 256 results */
	bb = bb + cc;   /* then combined with bb */

	for (i = 0; i < 256; ++i)
	{
		x = mm[i];
		switch (i % 4)
		{
		case 0: aa = aa ^ (aa << 13); break;
		case 1: aa = aa ^ (aa >> 6); break;
		case 2: aa = aa ^ (aa << 2); break;
		case 3: aa = aa ^ (aa >> 16); break;
		}
		aa = mm[(i + 128) % 256] + aa;
		mm[i] = y = mm[(x >> 2) % 256] + aa + bb;
		randrsl[i] = bb = mm[(y >> 10) % 256] + x;

		/* Note that bits 2..9 are chosen from x but 10..17 are chosen
		   from y.  The only important thing here is that 2..9 and 10..17
		   don't overlap.  2..9 and 10..17 were then chosen for speed in
		   the optimized version (rand.c) */
		   /* See http://burtleburtle.net/bob/rand/isaac.html
			  for further explanations and analysis. */
	}
}


/* if (flag!=0), then use the contents of randrsl[] to initialize mm[]. */
#define mix(a,b,c,d,e,f,g,h) \
{ \
   a^=b<<11; d+=a; b+=c; \
   b^=c>>2;  e+=b; c+=d; \
   c^=d<<8;  f+=c; d+=e; \
   d^=e>>16; g+=d; e+=f; \
   e^=f<<10; h+=e; f+=g; \
   f^=g>>4;  a+=f; g+=h; \
   g^=h<<8;  b+=g; h+=a; \
   h^=a>>9;  c+=h; a+=b; \
}

void randinit(flag)
int flag;
{
	int i;
	ub4 a, b, c, d, e, f, g, h;
	aa = bb = cc = 0;
	a = b = c = d = e = f = g = h = 0x9e3779b9;  /* the golden ratio */

	for (i = 0; i < 4; ++i)          /* scramble it */
	{
		mix(a, b, c, d, e, f, g, h);
	}

	for (i = 0; i < 256; i += 8)   /* fill in mm[] with messy stuff */
	{
		if (flag)                  /* use all the information in the seed */
		{
			a += randrsl[i]; b += randrsl[i + 1]; c += randrsl[i + 2]; d += randrsl[i + 3];
			e += randrsl[i + 4]; f += randrsl[i + 5]; g += randrsl[i + 6]; h += randrsl[i + 7];
		}
		mix(a, b, c, d, e, f, g, h);
		mm[i] = a; mm[i + 1] = b; mm[i + 2] = c; mm[i + 3] = d;
		mm[i + 4] = e; mm[i + 5] = f; mm[i + 6] = g; mm[i + 7] = h;
	}

	if (flag)
	{        /* do a second pass to make all of the seed affect all of mm */
		for (i = 0; i < 256; i += 8)
		{
			a += mm[i]; b += mm[i + 1]; c += mm[i + 2]; d += mm[i + 3];
			e += mm[i + 4]; f += mm[i + 5]; g += mm[i + 6]; h += mm[i + 7];
			mix(a, b, c, d, e, f, g, h);
			mm[i] = a; mm[i + 1] = b; mm[i + 2] = c; mm[i + 3] = d;
			mm[i + 4] = e; mm[i + 5] = f; mm[i + 6] = g; mm[i + 7] = h;
		}
	}

	isaac();            /* fill in the first set of results */
	randcnt = 256;        /* prepare to use the first set of results */
}

void sig_handler(int signo)
{
  if(signo == SIGTERM)
  {
    syslog(LOG_INFO, "SIGTERM has been caught! Exiting...");
    if(remove("run/daemon.pid") != 0)
    {
      syslog(LOG_ERR, "Failed to remove the pid file. Error number is %d", errno);
      exit(1);
    }
    exit(0);
  }
}

void handle_signals()
{
  if(signal(SIGTERM, sig_handler) == SIG_ERR)
  {
    syslog(LOG_ERR, "Error! Can't catch SIGTERM");
    exit(1);
  }
}

void daemonise()
{
  pid_t pid, sid;
  FILE *pid_fp;

  syslog(LOG_INFO, "Starting daemonisation.");

  //First fork
  pid = fork();
  if(pid < 0)
  {
    syslog(LOG_ERR, "Error occured in the first fork while daemonising. Error number is %d", errno);
    exit(1);
  }

  if(pid > 0)
  {
    syslog(LOG_INFO, "First fork successful (Parent)");
    exit(0);
  }
  syslog(LOG_INFO, "First fork successful (Child)");

  //Create a new session
  sid = setsid();
  if(sid < 0) 
  {
    syslog(LOG_ERR, "Error occured in making a new session while daemonising. Error number is %d", errno);
    exit(1);
  }
  syslog(LOG_INFO, "New session was created successfuly!");

  //Second fork
  pid = fork();
  if(pid < 0)
  {
    syslog(LOG_ERR, "Error occured in the second fork while daemonising. Error number is %d", errno);
    exit(1);
  }

  if(pid > 0)
  {
    syslog(LOG_INFO, "Second fork successful (Parent)");
    exit(0);
  }
  syslog(LOG_INFO, "Second fork successful (Child)");

  pid = getpid();

  //Change working directory to Home directory
  if(chdir(getenv("HOME")) == -1)
  {
    syslog(LOG_ERR, "Failed to change working directory while daemonising. Error number is %d", errno);
    exit(1);
  }

  //Grant all permisions for all files and directories created by the daemon
  umask(0);

  //Redirect std IO
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  if(open("/dev/null",O_RDONLY) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stdin while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(open("/dev/null",O_WRONLY) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stdout while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(open("/dev/null",O_RDWR) == -1)
  {
    syslog(LOG_ERR, "Failed to reopen stderr while daemonising. Error number is %d", errno);
    exit(1);
  }

  //Create a pid file
  mkdir("run/", 0777);
  pid_fp = fopen("run/daemon.pid", "w");
  if(pid_fp == NULL)
  {
    syslog(LOG_ERR, "Failed to create a pid file while daemonising. Error number is %d", errno);
    exit(1);
  }
  if(fprintf(pid_fp, "%d\n", pid) < 0)
  {
    syslog(LOG_ERR, "Failed to write pid to pid file while daemonising. Error number is %d, trying to remove file", errno);
    fclose(pid_fp);
    if(remove("run/daemon.pid") != 0)
    {
      syslog(LOG_ERR, "Failed to remove pid file. Error number is %d", errno);
    }
    exit(1);
  }
  fclose(pid_fp);
}

int sizeOfFile(FILE *buffer)
{
  int size, curPos;
  curPos = ftell(buffer);
  fseek(buffer, 0, SEEK_END);
  size = ftell(buffer);
  fseek(buffer, curPos, SEEK_SET);
  return size;
}

void rndgnt()
{
	ub4 i, j;
	aa = bb = cc = (ub4)0;
	for (i = 0; i < 256; ++i) mm[i] = randrsl[i] = (ub4)0;
        randinit(1);
	for (i = 0; i < 2; ++i)
	{
		isaac();
	}
}


void fillData(int size, FILE *buffer)
{
  syslog(LOG_INFO, "fillData()"); 
  struct sysinfo sys_info;
  time_t rawtime;
  int k = 0, num = 0;
  ub4 j;
  while (sizeOfFile(buffer) < size)
  {
    rndgnt();
    sysinfo(&sys_info);
    for(j = 0; j < 256; ++j)
    {
	time(&rawtime);
	ub4 result = randrsl[j] ^ rawtime ^ sys_info.freeram ^ sys_info.bufferram ^ sys_info.freeswap;
        fwrite(&result,sizeof(ub4),1,buffer); 
    }
    //fwrite(&num, sizeof(int), 1, buffer);
  } 
}

// сделать функцию для подсчета размера
void randomize()
{
  FILE *buffer;
  int size;
  syslog(LOG_INFO, "random");
  chdir(getenv("HOME"));
  if(mkdir("random", S_IRWXU | S_IRWXG | S_IRWXO))
  {
    syslog(LOG_INFO, "dir is already create");
  }
  if((buffer = fopen("random/buffer", "r")) == NULL)
  {
    syslog(LOG_INFO, "not exist buffer file, create new file");
    buffer = fopen("random/buffer", "a");
    size = sizeOfFile(buffer);
    syslog(LOG_INFO, "size of empty file is %i byte", size);
    fillData(5*1024*1024, buffer);
  }
  else
  {
    syslog(LOG_INFO, "exist buffer file");
    buffer = fopen("random/buffer", "a");
    //fseek(buffer, 0 , SEEK_END);
    //size = ftell(buffer); 
    size = sizeOfFile(buffer);
    syslog(LOG_INFO, "size is %i", size);
    if (size < 5*1024*1024)
    {
      fillData(5 * 1024*1024, buffer);
      syslog(LOG_INFO, "size is less than 5 mb");
    }
    else
    {
      syslog(LOG_INFO, "size is more than 5 mb, do nothing");  
      //do nothing    
    }
    fclose(buffer);
  }
  
  
}

int main(int argc,char *argv[])
{
  pid_t pid;
  int fd, len;
  FILE *pid_fp;
  char pid_buf[16];
  if (argc == 2) 
  {
    //syslog(LOG_INFO, "2 arg");
    if(!strcmp(argv[1], "start"))
    {
      //syslog(LOG_INFO, "start");
      chdir(getenv("HOME"));
      if((pid_fp = fopen("run/daemon.pid", "r")) == NULL)
      {
        //syslog(LOG_INFO, "create daemon.pid");
        daemonise();
        handle_signals();
      }
      else
      {
        //syslog(LOG_INFO, "daemon.pid is exist");
        fclose(pid_fp);
        exit(0);
      }
    }
    if(!strcmp(argv[1], "stop"))
    {
       //syslog(LOG_INFO, "Stop");
       chdir(getenv("HOME"));
       if((pid_fp = fopen("run/daemon.pid", "r")) == NULL)
       {
          //syslog(LOG_INFO, "already killed");
          exit(0); 
       }
       else
       {
         //syslog(LOG_INFO, "find pid");
         fscanf(pid_fp, "%i", &pid);
         //len = read(fd, pid_buf, 16);
         //pid_buf[len] = 0;
         //pid = atoi(pid_buf);
         //syslog(LOG_INFO, "pid is %i", pid );
         kill(pid, SIGTERM);
         fclose(pid_fp);
         exit(0);
       }
     }
  }
  else
  {
    exit(0);
  }

  while(1)
  {
    randomize();
    sleep(5);
  }
  return 0;
}
