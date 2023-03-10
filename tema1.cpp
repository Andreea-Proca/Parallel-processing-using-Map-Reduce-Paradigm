#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>
#include <iterator>
#include <vector>
#include <fstream>
#include <string>
#include <string.h>
#include <algorithm>
#include <math.h>
#include <queue>
#include <limits.h>
#include <set>
#include <unordered_set>
using namespace std;


pthread_mutex_t mutexMap;
pthread_barrier_t barrierReduce;

struct help {
    int *thread_id; 
    int M, R;
    vector<vector<unordered_set<int>>> *mappedList;
    queue<pair<string, int>> *q;
    vector<unordered_set<int>> *puteri;
};

bool cmp(pair<string, int>& a, pair<string, int>& b) {
    return a.second > b.second;
}

vector<unordered_set<int>> calcPuteri(int R) {
    vector<unordered_set<int>> puteri;  // lista de puteri
    // pentru fiecare putere pana la cea maxima
    for (int j = 2; j <= R+1; j++) {
        unordered_set<int> se;
    // se ridica la puterea j fiecare numar pana este depasita limita INT_MAX
        for (int i = 2; pow(i, j) < INT_MAX; i++) {
            se.insert(pow(i, j));  // nr obtinut se adauga la lista(set) puterii j
        }
       puteri.push_back(se); // lista este adaugata la lista(vector) de puteri
    }
    return puteri;
}

void check_pow (int e, void* arg) {
    struct help helper = *(struct help*)arg;
    if (e == 1) {
        // se adauga elementul la listele tuturor puterilor pana la cea maxima
        for (int i = 2; i <= helper.R+1; i++)
            helper.mappedList->at(*(helper.thread_id))[i-2].insert(e);
    } else {
        for (int j = 2; j <= helper.R+1; j++) {
            // daca elementul se afla in lista puterii j
            // (lista se afla la index j-2)
            if (helper.puteri->at(j-2).find(e) != helper.puteri->at(j-2).end())
                // este adaugat in lista puterii j a thread-ului cu ID dat
                helper.mappedList->at(*(helper.thread_id))[j-2].insert(e);
        }       
    }
}

void myMap(void *arg, string key)
{
    ifstream f;
    f.open(key);  // deschide fisierul dat
    int n, e;
    f >> n;  // nr de elemente din fisier
    // citeste si apeleaza functia de verificare putere pentru fiecare element
    for( int i = 0; i < n; i++) {
        f >> e;
        check_pow(e, arg);
    }
    f.close();  // inchide fisierul dat
}

void myReduce(void *arg)
{   
    struct help helper = *(struct help*)arg;
    std::unordered_set<int> s1;
    // indexul listei puterii corespunzatoare thread-ului Reduce
    int p = *(helper.thread_id) - helper.M;
    // pentru fiecare lista Map rezultata
    for(int j = 0; j < helper.M; j++) {
        // pun intr-un set lista puterii corespunzatoare thread-ului Reduce
        // care a intrat in functie (primul thread Reduce are puterea 2 etc)
        s1.insert(helper.mappedList->at(j)[p].begin(), 
                    helper.mappedList->at(j)[p].end());
    }
    // creez numele fisierului out in functie de putere(= index putere + 2)
    string out = "out" + std::to_string(p + 2) + ".txt";
    ofstream fp;
	fp.open(out);
    // pun in fisier dimensiunea set-ului nou format = nr de elemente unice
    fp << s1.size();
    fp.close();
}

void *thread_function(void *arg)
{
    struct help helper = *(struct help*)arg;
    if (*(helper.thread_id) < helper.M) {  // daca thread-ul este Map
        string key;
        while (!helper.q->empty()) {  // cat timp coada este goala
            pthread_mutex_lock(&mutexMap);  // blochez cu mutex
            if(!helper.q->empty()) {  // daca coada nu este goala
            // iau cheia primulului element din coada
            // (= numele fisierului de citit)
                key = helper.q->front().first;
                helper.q->pop(); // scot elementul din coada
                pthread_mutex_unlock(&mutexMap);  // deblochez mutex
                myMap(arg, key);  // apelez functia de mapare
            } else {
                pthread_mutex_unlock(&mutexMap); // deblochez mutex
                break;  // coada e goala si ies din while 
            }
        }
    }
    // bariera asteapta ca toate thread-urile sa ajunga aici
    // pentru a le lasa sa treaca
    pthread_barrier_wait(&barrierReduce);
    if (*(helper.thread_id) >= helper.M) {  // daca thread-ul este Reduce
       myReduce(arg);  // apelez functia de reducere
    }
    pthread_exit(NULL);  // thread-urile se opresc
}

int main(int argc, char *argv[]) {

    int M = atoi(argv[1]); // nr. de thread-uri Map
    int R = atoi(argv[2]); // nr. de thread-uri Reduce
    const char* S = argv[3]; //fisierul din care se citeste
    int i, r;
    void *status;
    pthread_t tid[M+R]; // vectorul de thread-uri
	int thread_id[M+R]; // vectorul de thread IDs
    int N;  // nr de nume de fisiere de citit
    pthread_mutex_init(&mutexMap, NULL); // initializez mutexul
    pthread_barrier_init(&barrierReduce, NULL, M+R); // initializez bariera cu
                                                    // nr total de thread-uri

    vector<vector<unordered_set<int>>> mappedList;  // initializare liste Map
    for (int i = 0; i < M; i++) {
        vector<unordered_set<int>> lista1 ;
        for (int j = 0; j < R; j++) {
            unordered_set<int> lista2;
            lista1.push_back(lista2);
        }
        mappedList.push_back(lista1);
    }

    // Citirea din fisierul principal
    ifstream f;
	f.open(S);
    f >> N;
    vector<pair<string, int>> order; // vectorul in care vor fi perechile
                                    //fisier-dimensiune
    string str;  // numele fisierului
    for (int i = 0; i < N; i++) {
        f >> str;
        ifstream fp;
        fp.open(str);  // deschid fisierul
        fp.seekg(0, ios::end);  // caut finalul fisierului
        int file_size = fp.tellg(); // calculez dimensiunea
        fp.close();  // inchid fisierul
        order.push_back(make_pair(str, file_size));  // retin in vector
    }
    f.close();

    sort(order.begin(), order.end(), cmp);  // sortez descrescator
    queue<pair<string, int>> q;  // creez coada
    for (auto& it : order) {  // pun in coada fiecare element din vector
        q.push(it);
    }
  
    struct help structs[M+R];  // vector de structuri help cu nr de elemente
                              //egal cu nr total de thread-uri
    vector<unordered_set<int>> puteri = calcPuteri(R); // calculez toate puterile
    for (i = 0; i < M+R; i++) {
        // initializez o structura help pentru noul thread
        help helper;
        helper.M = M;
        helper.R = R;
        helper.mappedList = &mappedList;
        helper.q = &q;
        thread_id[i] = i;
        helper.thread_id = &thread_id[i];
        helper.puteri = &puteri;
        structs[i] = helper;  // pun structura in vectorul de structuri
		r = pthread_create(&tid[i], NULL, thread_function, &structs[i]); // creez thread

		if (r) {
			printf("Eroare la crearea thread-ului %d\n", i);
			exit(-1);
		}
	}

    for (i = 0; i < M+R; i++) {
		r = pthread_join(tid[i], &status);

		if (r) {
			printf("Eroare la asteptarea thread-ului %d\n", i);
			exit(-1);
		}
	}

    pthread_mutex_destroy(&mutexMap);  // distrug mutexul
    pthread_barrier_destroy(&barrierReduce);  // distrug bariera
    return 0;
}