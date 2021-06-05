#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <stdint.h>
#include <string>
#include <string.h>
#include <sys/time.h>

int pthread_sleep_cond(double seconds);
int pthread_sleep(double seconds);
using namespace std;

/* Default values for when user doesn't explicitly gives arguments */
int N = 4;  /* Number of commentators. */
int q = 5; /* Number of questions that will be asked */
float p = 0.75; /* Probability of a commentator to answer a question */
int t = 3; /* Maximum number of seconds that a commentator can speak */
float b = 0.05; /* Probability of getting a breaking news every second. */

int ready_commentators = 0; /* Number of commentators that are processed before starting to answer the question */
int questions_asked = 0; /* Number of questions asked */

bool end_program = false; /* Status of the program */
bool incoming_breaking_news = false; /* Boolean showing whether an breaking new has arrived */

pthread_cond_t* commentator_conds; /* Conditions pointer to hold the commentators' conditions. */

pthread_mutex_t breaking_news_mutex;/* Mutex for breaking news */
pthread_cond_t breaking_news_cond;/* Condition for breaking news */

pthread_mutex_t moderator_mutex;/* Mutex for moderator */
pthread_cond_t moderator_cond;/* Condition for moderator */

pthread_cond_t commentator_interrupt_cond; /*Condition to interrupt speaking commentator when a breaking new arrives */
pthread_cond_t start_cond; /* Condition for breaking news thread start */
pthread_mutex_t start_mutex;/* Mutex for breaking news thread start */
pthread_mutex_t new_question_mutex;/* Mutex for new question */
pthread_cond_t new_question_cond;/* Condition for new question */

pthread_mutex_t queue_lock;/* Mutex to lock the commentator queue */

struct timeval start_time;/* Start time of the program for logging purposes */

struct Commentator {
    int id;
    pthread_mutex_t lock;
} typedef Commentator;

queue<Commentator> commentator_queue;/* Queue of commentators that will answer the question */

char timestamp[12];
long millisecond_start;

/* Current time formatter. By finding the difference between the start time and now,
it finds the passed time and returns that time in minutes:seconds.milliseconds format*/
char *timeFormatter(){
    struct timeval current;
    gettimeofday(&current, NULL);

    long millisecond_current = current.tv_sec * 1000 + current.tv_usec / 1000;
    int milliseconds = millisecond_current - millisecond_start;
    int seconds = milliseconds / 1000;
    milliseconds %= 1000;
    int minutes = seconds / 60;
    seconds %= 60;

    sprintf(timestamp, "[%02d:%02d.%03d]", minutes, seconds, milliseconds);
    return timestamp;
}

/* Arguments parser. If any of the following arguments are not given, that argument
starts with the default value given in the beginning.*/
int parse_cmd(int argc, char *argv[])
{
    for(int i = 0; i < argc; i++){
        if(strcmp(argv[i], "-n") == 0){
            N = atoi(argv[i+1]);
            i++;
        }
        else if(strcmp(argv[i], "-q") == 0){
            q = atoi(argv[i+1]);
            i++;
        }
        else if(strcmp(argv[i], "-p") == 0){
            p = atof(argv[i+1]);
            i++;
        }
        else if(strcmp(argv[i], "-t") == 0){
            t = atoi(argv[i+1]);
            i++;
        }
        else if(strcmp(argv[i], "-b") == 0){
            b = atof(argv[i+1]);
            i++;
        }
    }
    return 1;
}

/* COMMENTATOR FUNCTION */
void *commentator_func(void *commentator)
{
    /* Gets the commentator struct from the main  */
    Commentator comm;
    memcpy(&comm, (Commentator*)commentator, sizeof comm);

    /* Uses queue since an increasing job is happening. */
    pthread_mutex_lock(&queue_lock);
    ready_commentators++;
    /* When all of the commentators are created and ready, signals the moderator to start. */
    if(ready_commentators==N){
        pthread_cond_broadcast(&moderator_cond);
        ready_commentators=0;
    }
    pthread_mutex_unlock(&queue_lock);

    
    while(!end_program){

        /* Waits for a question */
        pthread_mutex_lock(&new_question_mutex);
        pthread_cond_wait(&new_question_cond, &new_question_mutex); /* Wait for question to be asked */
        pthread_mutex_unlock(&new_question_mutex);

        /* If program has ended, exists the thread*/
        if(end_program) exit(0); 

        /* Creates a random double between 0.0 and 1.0*/
        double random = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

        if(random <= p){ /* If commentator has decided to answer, enters if */
           
            /*Locks the queue since a new id will be pushed there.*/
            pthread_mutex_lock(&queue_lock);
            commentator_queue.push(comm);
            printf("%s Commentator #%d generates answer, position in queue: %lu\n", timeFormatter(), (int)comm.id+1, commentator_queue.size()-1);
            ready_commentators++; 
            
            /*If all commentators have decided whether they will answer the question,
             signals the moderator to start speaking*/
            if(ready_commentators==N){
                pthread_cond_signal(&moderator_cond);
                ready_commentators=0;
            }
            pthread_mutex_unlock(&queue_lock);
            
            /* Waits for moderator to let them speak */
            pthread_cond_wait(&commentator_conds[comm.id], &comm.lock); 
            double t_speak = 1 + static_cast <float> (rand()) / ( static_cast <float> (RAND_MAX/(t-1)));
            printf("%s Commentator #%d's turn to speak for %.3f seconds\n", timeFormatter(), (int)comm.id+1, t_speak);
            
            /* Uses cond_timedwait sleep function to be able to be interrupted by breaking news */
            pthread_sleep_cond(t_speak);

            /* If there is a breaking news, speaking of the commentator is cut short */
            if(incoming_breaking_news){
                printf("%s Commentator #%d is cut short due to a breaking news\n", timeFormatter(), (int)comm.id+1);
            } else {
                printf("%s Commentator #%d finished speaking\n", timeFormatter(), (int)comm.id+1);
                pthread_cond_signal(&moderator_cond); /* Signals moderator when speaking is finished */
            }
        } else {
            /* If the commentator doesn't answer the question, it just increases
            the number of ready commentators. */
            pthread_mutex_lock(&queue_lock);
            ready_commentators++;
            /*If all commentators have decided whether they will answer the question,
            signals the moderator to start speaking*/
            if(ready_commentators==N){
                pthread_cond_signal(&moderator_cond);
                ready_commentators=0;
            }
            pthread_mutex_unlock(&queue_lock);
        }
    }

    exit(0);
}

/* MODERATOR FUNCTION */
void *mod_func(void *param)
{ 
    /* Waits for moderator condition to be signaled to start*/
    pthread_mutex_lock(&moderator_mutex);
    pthread_cond_wait(&moderator_cond, &moderator_mutex);
    pthread_mutex_unlock(&moderator_mutex);
    /* Signals breaking news thread to start*/
    pthread_cond_signal(&start_cond);
    /* While we still have questions to ask, stays in the loop */
    while(questions_asked < q){
        questions_asked++;
        printf("%s Moderator asks question %d\n", timeFormatter(), questions_asked);

        /* Broadcasts all of the commentator threads that a new question is asked. */
        pthread_cond_broadcast(&new_question_cond); 

        /* Waits for commentators to signal back to start calling the commentators to speak*/
        pthread_cond_wait(&moderator_cond, &moderator_mutex);

        int q_size = commentator_queue.size();
        for(int i=0; i < q_size; i++){
            /* Pops the commentator in the queue one by one*/
            Commentator comm = commentator_queue.front();
            /* If there is a breaking news, it waits in the while loop
            before signaling the next commentator. */
            while(incoming_breaking_news);
            /* Signals the next commentator to speak */
            pthread_cond_signal(&commentator_conds[comm.id]);
            /* Waits for the commentator and moves on to the other person in the for loop */
            pthread_cond_wait(&moderator_cond, &moderator_mutex);
            /* Removes the commentator who just spoke from the queue. */
            commentator_queue.pop();
        }
    }
    printf("%s Simulation finished.\n",timeFormatter());
    /* Changes the end program to true to finish other threads */
    end_program = true;
    pthread_cond_signal(&new_question_cond); /*signal to end the program */
    exit(0);
}

/* BREAKING NEWS FUNCTION */
void *breaking_news_func(void *param)
{ 
    /* Waits for moderator to signal and start the breaking news thread */
    pthread_cond_wait(&start_cond,&start_mutex);
    while(!end_program){

        /* Waits in the while loop when the program has not ended */
        if(incoming_breaking_news){
            printf("%s Breaking news!\n", timeFormatter());

            /* If there is an incoming breaking news, signals the commentator interrupt
            in the pthread_sleep_cond function to interrupt the speaking commentator */
            pthread_cond_signal(&commentator_interrupt_cond);
            pthread_sleep(5);
            printf("%s Breaking news ends\n", timeFormatter());
            /* Turns the incoming breaking news to false */
            incoming_breaking_news = 0;
            /* Signals the moderator to continue with the next commentator */
            pthread_cond_signal(&moderator_cond); /*signal moderator breaking news finished */
            pthread_cond_signal(&breaking_news_cond);
        }
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    /* Initialize the start time and start time milliseconds */
    gettimeofday(&start_time, NULL);
    millisecond_start = start_time.tv_sec * 1000 + start_time.tv_usec / 1000;
    
    parse_cmd(argc,argv);
    /* Allocate the memory for commentator conditions */
    commentator_conds = (pthread_cond_t*) malloc(N * sizeof(pthread_cond_t));
    
    /* Initialize the mutex and conditions */
    pthread_mutex_init(&breaking_news_mutex, NULL);
    pthread_cond_init(&breaking_news_cond, NULL);

    pthread_mutex_init(&start_mutex, NULL);
    pthread_cond_init(&start_cond, NULL);

    pthread_cond_init(&commentator_interrupt_cond, NULL);

    pthread_mutex_init(&moderator_mutex, NULL);
    pthread_cond_init(&moderator_cond, NULL);

    pthread_mutex_init(&new_question_mutex, NULL);
    pthread_cond_init(&new_question_cond, NULL);

    pthread_mutex_init(&queue_lock, NULL);


    Commentator commentator_array[N];
    pthread_t moderator, commentator[N], breaking_news;
    /* Seed for randomness */
    time_t t;
    srand((unsigned) time(&t));

    /* Starts and creates the moderator thread first */
    pthread_create(&moderator, NULL, mod_func, NULL);
    /* Creates the commentator structs, conditions and threads. */
    for (int i = 0; i < N; i++){
        Commentator comm;
        comm.id = i;
        pthread_mutex_init(&comm.lock, NULL);
        pthread_cond_init(&commentator_conds[i], NULL);
        commentator_array[i] = comm;
        pthread_create(&commentator[i], NULL, commentator_func, (void *) &commentator_array[i]);  
    }
    pthread_create(&breaking_news, NULL, breaking_news_func, NULL); /*start breaking news thread */

    /* Checks whether there is a breaking news every second until the program ends */
    while(!end_program){
        double random = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        if(random <= b){
            incoming_breaking_news = 1;
            pthread_cond_signal(&start_cond);
            /* Waits for the signal from breaking news function to continue with the loop */
            pthread_cond_wait(&breaking_news_cond, &breaking_news_mutex);
        }
        /* Sleeps for 1 second and does the same things over and over every second.*/
        pthread_sleep(1);
    }

    /* Joins the threads together */
    pthread_join(moderator, NULL);
    for (int i = 0; i < N; i++) {
        pthread_join(commentator[i], NULL);
    }

    /* Frees and destroy allocated memories, mutexes and conditions */ 
    free(commentator_conds);
    pthread_cond_destroy(&breaking_news_cond);
    pthread_cond_destroy(&moderator_cond);
    pthread_cond_destroy(&new_question_cond);
        
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&breaking_news_mutex);
    pthread_mutex_destroy(&moderator_mutex);
    pthread_mutex_destroy(&new_question_mutex);
    return 0;
}

/**
 * updated sleep, changed
 * pthread_sleep takes an integer number of seconds to pause the current thread
 * used to sleep the commentators but still be aware when a breaking news comes
 * original by Yingwu Zhu
 * updated by Muhammed Nufail Farooqi
 * updated by Fahrican Kosar
 * updated by Doğa Demirtürk (to use pthread_cond_timedwait)
 */
int pthread_sleep_cond(double seconds){
    pthread_mutex_t mutex;
    if(pthread_mutex_init(&mutex,NULL)){
        return -1;
    }

    struct timeval tp;
    struct timespec timetoexpire;
    /* When to expire is an absolute time, so get the current time and add */
    /* it to our delay time */
    gettimeofday(&tp, NULL);
    long new_nsec = tp.tv_usec * 1000 + (seconds - (long)seconds) * 1e9;
    timetoexpire.tv_sec = tp.tv_sec + (long)seconds + (new_nsec / (long)1e9);
    timetoexpire.tv_nsec = new_nsec % (long)1e9;

    pthread_mutex_lock(&mutex);
    int res = pthread_cond_timedwait(&commentator_interrupt_cond, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);

    /*Upon successful completion, a value of zero shall be returned */
    return res;
}

/**
 * provided pthread sleep, unchanged
 * pthread_sleep takes an integer number of seconds to pause the current thread
 * original by Yingwu Zhu
 * updated by Muhammed Nufail Farooqi
 * updated by Fahrican Kosar
 */
int pthread_sleep(double seconds){
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    if(pthread_mutex_init(&mutex,NULL)){
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL)){
        return -1;
    }

    struct timeval tp;
    struct timespec timetoexpire;
    /* When to expire is an absolute time, so get the current time and add */
    /* it to our delay time */
    gettimeofday(&tp, NULL);
    long new_nsec = tp.tv_usec * 1000 + (seconds - (long)seconds) * 1e9;
    timetoexpire.tv_sec = tp.tv_sec + (long)seconds + (new_nsec / (long)1e9);
    timetoexpire.tv_nsec = new_nsec % (long)1e9;

    pthread_mutex_lock(&mutex);
    int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);

    /*Upon successful completion, a value of zero shall be returned */
    return res;
}