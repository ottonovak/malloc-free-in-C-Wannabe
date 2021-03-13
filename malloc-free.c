#include <stdio.h>
#include <string.h>
#include <httpext.h>
#include <time.h>
char *HEAP = NULL;

void vynulovat(void *ptr, int size)
{
    // funkcia pre vynolovanie daneho priestoru v poli
    for ( int i=0; i< size; i++)
        *(char*)(ptr + i) = 0;
}

void memory_init(void *ptr, unsigned int size)
{
    HEAP = (char*)ptr;
    *(int*)(HEAP) = size;
    *(void**)(HEAP + sizeof(int)) = HEAP + sizeof(int) + sizeof(void*); // ukazovatel na prazny blok
    *(int*)(HEAP + sizeof(int) + sizeof(void*)) = size - sizeof(int) - sizeof(void*);  // nepridelnena pamat hlavicka
    *(int*)(HEAP + size - sizeof(int)) = size - sizeof(int) - sizeof(void*);// nepridelnena pamat paticka

    vynulovat((void*)(HEAP + 2 * sizeof(int) + sizeof(char*)), size - 3 * sizeof(int) - sizeof(char*)); // pre prehladnost
}

void *memory_alloc(unsigned int size)
{
    if(size <= 0 || HEAP == NULL)
        return NULL;

    int new_block_size = size + 2 * sizeof(int);
    char *helpP = *(void**)(HEAP + sizeof(int));
    char *helpP2;

    while (helpP != NULL)
    {
        int free_size = *(int*)helpP;

        if(free_size >= new_block_size)
        {
            if(free_size - new_block_size > 2 * sizeof(int) + sizeof(char*))// ma miesto pre dalsi blok? ak nie, tak prideli cely blok
            {
                *(int*)helpP = new_block_size * (-1);// hlavicka noveho bloku1
                *(int*)(helpP + new_block_size - sizeof(int)) = new_block_size *(-1);// paticka noveho bloku1

                *(void**)(helpP + new_block_size + sizeof(int)) = *(void**)(helpP + sizeof(int));// presmerni novy smernik bloku2
                *(int*)(helpP + new_block_size) = free_size - new_block_size; // hlavicka noveho zviskoveho bloku2
                *(int*)(helpP + free_size - sizeof(int)) = free_size - new_block_size; // paticka noveho zvisneho bloku2
                *(void**)(HEAP + sizeof(int)) = (void*)(helpP + new_block_size);// presmeruje uakzovatel novej pamate na zaciatok bloku2
            }

            if(free_size - new_block_size <= 2 * sizeof(int) + sizeof(char*))
            {
                *(int*)helpP = free_size * (-1); // zapise obsadensot v hlavicky
                *(int*)(helpP + free_size - sizeof(int)) = free_size * (-1); // zapsiem obsadensot v paticky
                *(void**)(HEAP + sizeof(int)) = *(void**)(helpP + sizeof(int)); // presmerni uakzovatel novej pamate na dalsi volny blok
            }

            return (void*)(helpP + sizeof(int)); // vrati smernik na zaciatok alokovaneho bloku
        }

        helpP2 = (void*)(helpP + sizeof(int));
        helpP = *(void**)helpP2;    // cyklus prejde na dalsi volny blok

    }

    return NULL;
}

int memory_free(void *valid_ptr)
{
    int i;
    char *free = (char*)(valid_ptr - sizeof(int));
    int size = *(int*)free *(-1);
    int size_next = *(int*)(free +size);
    int size_prev = *(int*)(free - sizeof(int));
    char *prev = (void*)(free - size_prev);
    char *next = (void*)(free + size);

    // PRVY
    if((void*)free == (void*)(HEAP + 2 * sizeof(int)))
    {
        if(size_next < 0) // nasledovny je alokovany
        {
            *(void**)(free + sizeof(int)) = *(void**)(HEAP + sizeof(int)); // usmerni na nasledovny prazny blok
            *(void**)(free - sizeof(void*)) = (void*)free; // nasatvi ako prvny prazny blok
            *(int*)free = size;
            *(int*)(next - sizeof(int)) = size;
        }

        else// nasledovny je nealokovany
        {
            *(void**)(free + sizeof(int)) = *(void**)(free + size + sizeof(int)); // usmeri na dalsi prazny blok
            *(void**)(free - sizeof(void*)) = (void*)free; // nastavi ako prvny prazny blok
            *(int*)free = size + size_next; // hlavicka velkost
            *(int*)(next + size_next - sizeof(int)) = size + size_next; //paticka velkost

            vynulovat((void*)(next - sizeof(int)), 2 * sizeof(int) + sizeof(void *));
        }

        return 0;
    }

    // POSLEDNY
    if((void*)(next) == (void*)(HEAP + *(int*)HEAP))
    {
        if(size_prev < 0) // prepodsledny je alokovany
        {
            *(void**)(free + sizeof(int)) = *(void**)(HEAP + sizeof(int));
            *(void**)(HEAP + sizeof(int)) = free;
            *(int*)free = size;
            *(int*)(next - sizeof(int)) = size;
        }
        else // predosly je nealokovany
        {
            *(int*)(next - sizeof(int)) = size + size_prev;
            *(int*)prev = size + size_prev;

            vynulovat((void*)(free - sizeof(int)), 2 * sizeof(int));
        }

        return 0;
    }

    // V STREDE
    if( (size_prev < 0) && (size_next < 0))
    {
        *(void**)(free + sizeof(int)) = *(void**)(HEAP + sizeof(int));
        *(void**)(HEAP + sizeof(int)) = (void*)free;
        *(int*)free = size;
        *(int*)(next - sizeof(int)) = size;

        return 0;
    }

    if( size_prev > 0  && size_next < 0)
    {
        *(int*)(next - sizeof(int)) = size + size_prev;
        *(int*)(prev) = size + size_prev;

        vynulovat((void*)(free - sizeof(int)), 2 * sizeof(int));
        return 0;
    }

    if( size_prev > 0 && size_next > 0)
    {
        *(void**)(prev + sizeof(int)) = *(void**)(next + sizeof(int));
        *(int*)(prev) = size + size_prev + size_next;
        *(int*)(next + size_next - sizeof(int)) = size + size_prev + size_next;

        vynulovat((void*)(free - sizeof(int)), 2 * sizeof(int) + size + sizeof(void*));
        return 0;
    }

    if( size_prev < 0 && size_next > 0)
    {
        char *helpP = (void*)(HEAP + sizeof(int));

        while (*(void**)helpP != NULL) //cyklus cez nepridelne bloky
        {
            if(*(void**)helpP == next)
            {
                *(void**)helpP = free;
                *(int*)free = size + size_next;
                *(int*)(next + size_next - sizeof(int)) = size + size_next;
                *(void**)(free + sizeof(int)) = *(void**)(next + sizeof(int));

                vynulovat((void*)(next - sizeof(int)), 2 * sizeof(int) + sizeof(void*));
                break;
            }

            helpP = *(void**)helpP; // nasledovny volny blok
            helpP = (void*)(helpP + sizeof(int));
        }

        return 0;
    }

    return 1;
}

int memory_check(void *ptr)
{
    // ukezuje smernik do mojho pola?
    if(ptr == NULL || ((void*)(ptr - sizeof(int)) < (void*)(HEAP) + sizeof(int)) ||
       ((void*)(ptr - sizeof(int)) > (void*)(HEAP + *(int*)HEAP)))
        return 0;

    if((*(int*)(ptr - sizeof(int)) < 0)) // je blok alokovany?
        return 1;

    return 0;
}


void test(int all_size, int min, int max)
{
    int counter = 0, ideal_counter = 0, ideal =all_size;
    int block_size;
    int allocated_sum = 0;
    float percentage;
    char region[100000];
    char *pointer[10000];

    memory_init(region, all_size);
    srand(time(NULL));

    while (TRUE)
    {
        block_size =  (rand() % (max + 1 - min) + min);   //velksot alokovaneho bloku
        char *point = (char *)memory_alloc(block_size);
        if (point == NULL) //uz nie je miesto
            break;

        ideal -= block_size;
        allocated_sum += block_size;
        pointer[counter] = point;
        counter ++;
        ideal_counter ++;
    //    printf("%d ", size_block);
    }
    //    printf("\n");

    while(ideal >= min) // pokracuje alokovat pre idealny pripad
    {
        block_size =  (rand() % (max + 1 - min) + min);
        ideal -= block_size;
        ideal_counter ++;
    }

    percentage = (counter * 100 )/ ideal_counter;
    printf("Allocated %d blocks, ideal %d blocks, alocated %.2f%%%% blocks.\n",counter, ideal_counter,percentage);

    for(int i=0; i < counter; i++) // uvolnenie pridelenych blokov
        if(memory_check(pointer[i]))
            memory_free(pointer[i]);
}

void start_test()
{
    time_t start,stop;
    double duration;

    start = clock();
    printf("- - - - - - - SCENAR 1 - - - - - - -\n");
    for(int i=50; i <=200; i*=2)
    {
        printf("Size of all memory < %d >\nn",i);
        test(i, 8, 8);
        test(i, 15, 15);
        test(i, 24, 24);
    }
    stop = clock();
    duration = ( double ) ( stop - start ) / CLOCKS_PER_SEC;
    printf(">>>> It took %.4lf second to finish this scenary.\n\n", duration);


    start = clock();
    printf("- - - - - - - SCENAR 2 - - - - - - -\n");
    for(int i=50; i <=200; i*=2)
        test(i, 8, 24);
    stop = clock();
    duration = ( double ) ( stop - start ) / CLOCKS_PER_SEC;
    printf(">>>> It took %.4lf second to finish this scenary.\n\n", duration);


    start = clock();
    printf("- - - - - - - SCENAR 3 - - - - - - -\n");
    test(10000, 500, 5000);
    stop = clock();
    duration = ( double ) ( stop - start ) / CLOCKS_PER_SEC;
    printf(">>>> It took %.4lf second to finish this scenary.\n\n", duration);


    start = clock();
    printf("- - - - - - - SCENAR 4 - - - - - - -\n");
    test(100000, 8, 50000);
    stop = clock();
    duration = ( double ) ( stop - start ) / CLOCKS_PER_SEC;
    printf(">>>> It took %.4lf second to finish this scenary.\n\n", duration);
}

int main()
{

  start_test();

  // jednoduchy testovac
/*

    char region[130];
    memory_init(region, 130);

    char *p1 = (char *) memory_alloc(22);
    char *p2 = (char *) memory_alloc(22);
    char *p3 = (char *) memory_alloc(22);
    char *p4 = (char *) memory_alloc(24);
    memory_free(p4);
    memory_free(p3);
    memory_free(p1);
    memory_free(p2);

    if(memory_check(p2))
        printf("JE ALOKOVANY\n");

    for (int i = 0; i < 130; i++)
        printf("%d ",region[i]);
*/

    return 0;
}
