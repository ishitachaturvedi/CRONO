/*
    Distributed Under the MIT license
	Uses vertex coloring to distinguish searches
    Programs by Masab Ahmad (UConn)
*/

#include <cstdio>
#include <cstdlib>
#include <pthread.h>
//#include "carbon_user.h"     /* For the Graphite Simulator*/
#include <time.h>
#include <sys/timeb.h>

#define MAX            100000000
#define INT_MAX        100000000
// #define DEBUG              1
#define BILLION 1E9

int CHUNK_SIZE = 10;

//#define GEM5

#ifdef GEM5
#include "gem5/m5ops.h"
#endif

// Define global variables for the task queue
pthread_mutex_t task_queue_mutex;
int task_queue = 0;

// Declare other global variables
extern int largest;
extern int terminate;
extern int Total;
extern int* exist;
extern int* edges;
extern int** W_index;
extern int* temporary;
extern pthread_mutex_t* locks;

typedef struct
{
   int*      Q;
   int*      D;
   //int**     W;
   int**     W_index;
   int*      d_count;
   int       tid;
   int       P;
   int       N;
   int       DEG;
   pthread_barrier_t* barrier_total;
   pthread_barrier_t* barrier;
   int       *iterations;  // Add this to track iterations
} thread_arg_t;

//Function Initializers
int initialize_single_source(int* D, int* Q, int source, int N);
void init_weights(int N, int DEG, int** W, int** W_index);

//Global Variables
pthread_mutex_t lock;              //single lock
//pthread_mutex_t locks[4194304];  //locks for each vertes, upper limit
pthread_mutex_t *locks;
int Total = 0;
int terminate = 0;                 //work termination
int P_global = 256;
int *edges;                        //deg of a given vertex
int *exist;                        //whether vertex in graph
int *temporary;
int largest=0;
thread_arg_t thread_arg[1024];
pthread_t   thread_handle[1024];

// The work-stealing function
void* do_work(void* args)
{
   // Thread variables and arguments
   volatile thread_arg_t* arg = (thread_arg_t*) args;
   int tid                  = arg->tid;  // Thread ID
   int P                    = arg->P;    // Number of threads
   volatile int* Q          = arg->Q;    // Set/unset array
   int* D                   = arg->D;    // Coloring array
   int** W_index            = arg->W_index;  // Graph structure
   int v = 0;
   int iter = 0;

   pthread_barrier_wait(arg->barrier_total);

   while (terminate == 0) {
      int start;
      pthread_mutex_lock(&task_queue_mutex);
      start = task_queue;
      task_queue += 10;
      pthread_mutex_unlock(&task_queue_mutex);

      if (start >= largest + 1) break;
      int stop = (start + 9) > largest + 1 ? largest + 1 : (start + 9);

      for (v = start; v < stop; v++) {
         if (exist[v] == 0) continue;                              // If not in graph
         if (D[v] == 0 || D[v] == 2) continue;                    // Already colored

         for (int i = 0; i < edges[v]; i++) {
            int neighbor = W_index[v][i];
            if (Q[neighbor] == 1) {                       // Test and set
               pthread_mutex_lock(&locks[neighbor]);
               if (Q[neighbor] == 1)                       // If unset then set
                  Q[neighbor] = 0;                        // Can be set to Parent
               temporary[neighbor] = 1;
               pthread_mutex_unlock(&locks[neighbor]);
            }
         }
      }

      pthread_barrier_wait(arg->barrier_total);

      // Update colors	
      for (v = start; v < stop; v++) {
         if (D[v] == 1)
            D[v] = 2;
         else
            D[v] = temporary[v];
      }

      // Termination Condition
      if (Q[largest] == 0 || iter >= Total)
         terminate = 1;
      iter++;
      pthread_barrier_wait(arg->barrier_total);
   }

   pthread_barrier_wait(arg->barrier_total);

   return NULL;
}


//Main
int main(int argc, char** argv)
{
   FILE *file0 = NULL;
   int N = 0;
   int DEG = 0;
   //whether read from file or generate synthetic
   const int select = atoi(argv[1]);

   //if reading from file
   if(select==1)
   {
      const char *filename = argv[3];
      //printf("Please Enter The Name Of The File You Would Like To Fetch\n");
      //scanf("%s", filename);
      file0 = fopen(filename,"r");
   }

   int lines_to_check=0;
   char c;
   int number0;
   int number1;
   int inter = -1; 

   if(select==1)
   {
      N = 2097152; //can be read from file if needed, this is a default upper limit
      DEG = 16;     //also can be reda from file if needed, upper limit here again
   }

   //Max input threads
   const int P1 = atoi(argv[2]);

   int P = P1;
   P_global = P1;

   //If generating a uniform random graph
   if(select==0)
   {
      N = atoi(argv[3]);
      DEG = atoi(argv[4]);
      printf("\nGraph with Parameters: N:%d DEG:%d\n",N,DEG);
   } 

   if (DEG > N)
   {
      fprintf(stderr, "Degree of graph cannot be grater than number of Vertices\n");
      exit(EXIT_FAILURE);
   }

   //Memory Allocations
   int* D;
   int* Q;
   if(posix_memalign((void**) &D, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if(posix_memalign((void**) &Q, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if(posix_memalign((void**) &edges, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if(posix_memalign((void**) &exist, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if(posix_memalign((void**) &temporary, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   int d_count = N;
   pthread_barrier_t barrier_total;
   pthread_barrier_t barrier;

   int** W = (int**) malloc(N*sizeof(int*));
   int** W_index = (int**) malloc(N*sizeof(int*));
   for(int i = 0; i < N; i++)
   {
      //W[i] = (int *)malloc(sizeof(int)*N);
      if(posix_memalign((void**) &W[i], 64, DEG*sizeof(int)))
      {
         fprintf(stderr, "Allocation of memory failed\n");
         exit(EXIT_FAILURE);
      }
      if(posix_memalign((void**) &W_index[i], 64, DEG*sizeof(int)))
      {
         fprintf(stderr, "Allocation of memory failed\n");
         exit(EXIT_FAILURE);
      }
   }

   //Memory initialization
   for(int i=0;i<N;i++)
   {
      for(int j=0;j<DEG;j++)
      {
         //W[i][j] = 1000000000;
         W_index[i][j] = INT_MAX;
      }
      edges[i]=0;
      exist[i]=0;
      temporary[i]=0;
   }

   //If reading from file
   if(select==1)
   {
      for(c=getc(file0); c!=EOF; c=getc(file0))
      {
         if(c=='\n')
            lines_to_check++;

         if(lines_to_check>3)
         {
            int f0 = fscanf(file0, "%d %d", &number0,&number1);
            if(f0 != 2 && f0 != EOF)
            {
               printf ("Error: Read %d values, expected 2. Parsing failed.\n",f0);
               exit (EXIT_FAILURE);
            }
            //printf("\n%d %d",number0,number1);
            if(number0>largest)
               largest=number0;
            if(number1>largest)
               largest=number1;
            inter = edges[number0];

            //W[number0][inter] = drand48();
            W_index[number0][inter] = number1;
            //previous_node = number0;
            edges[number0]++;
            exist[number0]=1; exist[number1]=1;
         }
      }
      //printf("\n%d deg:%d",test[0]);
      printf("\nFile Read, Largest Vertex:%d",largest);
   }

   printf("HELLO1\n");

   //Generate Random graph
   if(select==0)
   {
      init_weights(N, DEG, W, W_index);
      largest = N-1; //largest vertex id
   }

   printf("HELLO2\n");

   //Synchronization variables
   pthread_barrier_init(&barrier_total, NULL, P);
   pthread_barrier_init(&barrier, NULL, P);
   locks = (pthread_mutex_t*) malloc((largest+16) * sizeof(pthread_mutex_t));
   pthread_mutex_init(&lock, NULL);

   printf("HELLO3\n");

   for(int i=0; i<largest+1; i++)
   {
      if(select==0)
      {
         exist[i] = 1;
         edges[i] = DEG;
      }
      if(exist[i]==1)
      {
         Total++;
         pthread_mutex_init(&locks[i], NULL);
      }
   }
   //printf("\n %d %d %d",N,largest,Total);

   printf("HELLO4\n");

   //Initialize Data Structures
   initialize_single_source(D, Q, 0, N);

   //Thread arguments
   for(int j = 0; j < P; j++) {
      thread_arg[j].Q          = Q;
      thread_arg[j].D          = D;
      thread_arg[j].W_index    = W_index;
      thread_arg[j].d_count    = &d_count;
      thread_arg[j].tid        = j;
      thread_arg[j].P          = P;
      thread_arg[j].N          = N;
      thread_arg[j].DEG        = DEG;
      thread_arg[j].barrier_total = &barrier_total;
      thread_arg[j].barrier    = &barrier;
      thread_arg[j].iterations = (int*)malloc(sizeof(int)); // Allocate memory for iteration count
   }

   printf("HELLO5\n");

   // Enable Graphite performance and energy models
   //CarbonEnableModels();

   //CPU Time
   // struct timespec requestStart, requestEnd;
   // clock_gettime(CLOCK_REALTIME, &requestStart);

   #ifdef GEM5
      m5_dump_reset_stats(0,0);
   #endif

   printf("HELLO6\n");
   fflush(stdout);

   //Spawn Threads
   for(int j = 1; j < P; j++) {
      pthread_create(thread_handle+j,
            NULL,
            do_work,
            (void*)&thread_arg[j]);
   }

   do_work((void*)&thread_arg[0]);

   //Join threads
   for(int j = 1; j < P; j++) { //mul = mul*2;
      pthread_join(thread_handle[j],NULL);
   }

   printf("HELLO7\n");
   fflush(stdout);

   #ifdef GEM5
      m5_dump_reset_stats(0,0);
   #endif

   printf("\nThreads Joined!");
   fflush(stdout);

   // clock_gettime(CLOCK_REALTIME, &requestEnd);
   // double accum = ( requestEnd.tv_sec - requestStart.tv_sec ) + ( requestEnd.tv_nsec - requestStart.tv_nsec ) / BILLION;
   // printf( "\nTime Taken:\n%lf seconds", accum );

   // // Print results
   // printf("Execution completed.\n");

   return 0;
}

int initialize_single_source(int*  D,
      int*  Q,
      int   source,
      int   N)
{
   for(int i = 0; i < N; i++)
   {
      D[i] = 0;
      Q[i] = 1;
   }

   D[source] = 1;
   Q[source] = 0;
   return 0;
}


void init_weights(int N, int DEG, int** W, int** W_index)
{
   // Initialize to -1
   for(int i = 0; i < N; i++)
      for(int j = 0; j < DEG; j++)
         W_index[i][j]= -1;

   // Populate Index Array
   for(int i = 0; i < N; i++)
   {
      int last = 0;
      for(int j = 0; j < DEG; j++)
      {
         if(W_index[i][j] == -1)
         {        
            int neighbor = i+j;
            //W_index[i][j] = i+j;//rand()%(DEG);

            if(neighbor > last)
            {
               W_index[i][j] = neighbor;
               last = W_index[i][j];
            }
            else
            {
               if(last < (N-1))
               {
                  W_index[i][j] = (last + 1);
                  last = W_index[i][j];
               }
            }
         }
         else
         {
            last = W_index[i][j];
         }
         if(W_index[i][j]>=N)
         {
            W_index[i][j] = N-1;
         }
      }
   }

   // Populate Cost Array
   for(int i = 0; i < N; i++)
   {
      for(int j = 0; j < DEG; j++)
      {
         double v = drand48();
         /*if(v > 0.8 || W_index[i][j] == -1)
           {       W[i][j] = MAX;
           W_index[i][j] = -1;
           }

           else*/ if(W_index[i][j] == i)
         W[i][j] = 0;

         else
            W[i][j] = (int) (v*100) + 1;
         //printf("   %d  ",W_index[i][j]);
      }
      //printf("\n");
   }
}
