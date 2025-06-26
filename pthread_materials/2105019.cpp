#include <iostream>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <random>
#include <unistd.h>
#include <chrono>
#include <string>

using namespace std;


#define MAX_WRITING_TIME 20
#define SLEEP_MULTIPLIER 1000
#define NUM_STATIONS 4
#define MAX_READERS 2

int N, M, x, y; 
int operations_completed = 0;


auto start_time = chrono::high_resolution_clock::now();
long long get_time()
{
    auto now = chrono::high_resolution_clock::now();
    return chrono::duration_cast<chrono::milliseconds>(now - start_time).count();
}


sem_t stations[NUM_STATIONS];
pthread_mutex_t log_mutex;
pthread_mutex_t group_locks[100]; 
pthread_mutex_t output_lock;
pthread_mutex_t reader_count_mutex;
sem_t logbook_write;
int reader_count = 0;


class Operative
{
public:
    int id, group_id, is_leader;
    Operative(int _id)
    {
        id = _id;
        group_id = (id - 1) / M;
        is_leader = ((id % M == 0) ? 1 : 0);
    }
};

vector<Operative> operatives;
bool group_finished[100];
int group_progress[100];

int get_random_poisson(double lambda = 10000.234)
{
    random_device rd;
    mt19937 gen(rd());
    poisson_distribution<int> dist(lambda);
    return dist(gen);
}


void log_output(const string &s)
{
    pthread_mutex_lock(&output_lock);
    cout << s << flush;
    pthread_mutex_unlock(&output_lock);
}


void *operative_task(void *arg)
{
    Operative *op = (Operative *)arg;
    usleep(get_random_poisson() * SLEEP_MULTIPLIER);

    int station_id = (op->id % NUM_STATIONS);
    sem_wait(&stations[station_id]);

    log_output("Operative " + to_string(op->id) + " has arrived at station " +
               to_string(station_id + 1) + " at time " + to_string(get_time()) + "\n");

    usleep(x * SLEEP_MULTIPLIER);

    log_output("Operative " + to_string(op->id) + " completed typing at station " +
               to_string(station_id + 1) + " at time " + to_string(get_time()) + "\n");

    sem_post(&stations[station_id]);

    pthread_mutex_lock(&group_locks[op->group_id]);
    group_progress[op->group_id]++;
    if (group_progress[op->group_id] == M)
    {
        group_finished[op->group_id] = true;
    }
    pthread_mutex_unlock(&group_locks[op->group_id]);


    if (op->is_leader)
    {
        while (!group_finished[op->group_id])
            usleep(1000);
        usleep(y * SLEEP_MULTIPLIER);

      
        sem_wait(&logbook_write);
        pthread_mutex_lock(&log_mutex);
        operations_completed++;
        log_output("Leader " + to_string(op->id) + " recorded in logbook at time " +
                   to_string(get_time()) + ". Completed Ops = " + to_string(operations_completed) + "\n");
        pthread_mutex_unlock(&log_mutex);
        sem_post(&logbook_write);
    }

    return NULL;
}

void *logbook_reader(void *arg)
{
    string name = *(string *)arg;
    while (true)
    {
        usleep(get_random_poisson() * 1000);

        pthread_mutex_lock(&reader_count_mutex);
        if (++reader_count == 1)
            sem_wait(&logbook_write);
        pthread_mutex_unlock(&reader_count_mutex);

        log_output(name + " is reading logbook at time " + to_string(get_time()) +
                   ". Completed Ops = " + to_string(operations_completed) + "\n");

        pthread_mutex_lock(&reader_count_mutex);
        if (--reader_count == 0)
            sem_post(&logbook_write);
        pthread_mutex_unlock(&reader_count_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: ./a.out <input_file>\n";
        return 1;
    }

    ifstream infile(argv[1]);
    infile >> N >> M >> x >> y;

    pthread_mutex_init(&output_lock, NULL);
    pthread_mutex_init(&log_mutex, NULL);
    pthread_mutex_init(&reader_count_mutex, NULL);
    sem_init(&logbook_write, 0, 1);

    for (int i = 0; i < NUM_STATIONS; i++)
        sem_init(&stations[i], 0, 1);
    for (int i = 0; i < 100; i++)
        pthread_mutex_init(&group_locks[i], NULL);

    for (int i = 1; i <= N; i++)
        operatives.emplace_back(i);

    pthread_t operative_threads[N];
    for (int i = 0; i < N; i++)
        pthread_create(&operative_threads[i], NULL, operative_task, &operatives[i]);

    
    string reader1 = "Staff 1", reader2 = "Staff 2";
    pthread_t readers[2];
    pthread_create(&readers[0], NULL, logbook_reader, &reader1);
    pthread_create(&readers[1], NULL, logbook_reader, &reader2);

    for (int i = 0; i < N; i++)
        pthread_join(operative_threads[i], NULL);

    
    pthread_cancel(readers[0]);
    pthread_cancel(readers[1]);

    return 0;
}
