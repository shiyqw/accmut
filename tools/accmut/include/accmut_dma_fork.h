#ifndef ACCMUT_DMA_FORK_H
#define ACCMUT_DMA_FORK_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <execinfo.h> //add for debug

#include <accmut/accmut_arith_common.h>

#define MUTFILELINE 128

typedef enum MTYPE{
	AOR,
	LOR,
	COR,
	ROR,
	SOR,
	STD,
	LVR
}MType;

typedef struct Mutation{
	MType type;
	int op;
	//for AOR, LOR
	int t_op;
	//for ROR
	int s_pre;
	int t_pre;
	//for STD
	int f_tp;
	//for LVR
	int op_index;
	long s_con;		// TODO: 'long' is enough ?
	long t_con;
}Mutation;

Mutation* ALLMUTS[MAXMUTNUM + 1];


/** Added By Shiyqw **/

int forked_active_set[21]; // 当前进程所包含的MUTATION ID
int forked_active_num; // 当前进程所包含的MUTATION ID的数量
int default_active_set[MAXMUTNUM + 1]; // 当前未激活过的所有MUTATION ID的位表，0代表已激活，1代表未激活
int recent_set[21]; // 当前激活的MUTATION ID
int recent_num; // 当前激活的MUTATION ID的个数
long temp_result[21]; // 保存临时计算结果

// 等价类的数据结构
typedef struct Eqclass {
    long value; // 代表这个等价类的计算结果
    int num; // 代表这个等价类中的元素个数
    int mut_id[21]; //代表这个等价类中的每个元素（都是一个Mutation ID）
} Eqclass;

Eqclass eqclass[21]; //每次划分至多21个等价类，其实最多应该只有十几个
int eq_num; //实时等价类的个数

// Algorithms for Dynamic mutation anaylsis 

// 此方法从当执行的进程所包含的所有MUTATION ID中筛选出在在from～to范围中的ID，结果存到resent_set中，个数存到recent_num中
void __accmut__filter__variant(int from, int to) {
    recent_num = 0; 
    int i;
    if (MUTATION_ID == 0) {
        // 如果当前为原进程，则从位表中选取
        recent_set[recent_num++] = 0;
        for(i = from; i <= to; ++i) {
            if (default_active_set[i] == 1) {
                recent_set[recent_num++] = i;
            }
        }
    } else {
        // 如果当前不是原进程，则从当前进程所包含的MUTATION ID中选取
        for(i = 0; i < forked_active_num; ++i) {
            if (forked_active_set[i] >= from && forked_active_set[i] <= to) {
                recent_set[recent_num++] = forked_active_set[i];
            }
        }
        if(recent_num == 0) {
            // 如果不是原进程且当前进程所包含的MUTATION ID中没有在from to之间的元素，那么表示当前进程在这句IR上没有变异，需要把原进程加入。
            recent_set[recent_num++] = 0;
        }
    }

}
//

// 根据已经得到的临时计算结果划分等价类
void __accmut__divide__eqclass() {
    eq_num = 0;
    int i;
    for(i = 0; i < recent_num; ++i) { // 对每一个临时计算结果
        long result = temp_result[i];
        int j;
        int flag = 0;
        for(j = 0; j < eq_num; ++j) { // 循环遍历已经划分出的等价类，看它们的值是否相等
            if(eqclass[j].value == result) {
                // 如果是，那么加入等价类。
                eqclass[j].mut_id[eqclass[j].num++] = recent_set[i];
                flag = 1;
                break;
            }
        }
        // 如果不属于任何一个等价类，那么自成为一个等价类
        if (flag == 0) {
            eqclass[eq_num].value = result;
            eqclass[eq_num].num = 1;
            eqclass[eq_num].mut_id[0] = recent_set[i];
            ++eq_num;
        }
    }
}

// 根据当前等价类筛选出当前进程所包含的MID
void __accmut__filter__mutants(int from, int to, int classid) {
    /** filter_mutants **/
    int j;
    if(eqclass[classid].mut_id[0] == 0) {
        // 如果当前为原进程，首先将位表中from到to全设为0（已激活），然后将当前等价类中所包含的其他进程设为1（未激活）
        for(j = from; j <= to; ++j) {
            default_active_set[j] = 0;
        }
        for(j = 0; j < eqclass[classid].num; ++j) {
           default_active_set[eqclass[classid].mut_id[j]] = 1;
        }
    } else {
        // 如果当前不是原进程，那么直接加入等价类中的所有ID
        forked_active_num = 0;
        for(j = 0; j < eqclass[classid].num; ++j) {
            forked_active_set[forked_active_num++] = eqclass[classid].mut_id[j];
        }
    }
}

// 根据等价类进行fork
long __accmut__fork__eqclass(int from, int to) {

    // 如果等价类个数为1，直接返回
    if(eq_num == 1) {
        return eqclass[0].value;
    }

    // 保存原结果
    int result = eqclass[0].value;
    int id = eqclass[0].mut_id[0];
    int i;
    
    /** fork **/
    for(i = 1; i < eq_num; ++i) { // 依次fork每个等价类
         int pid = 0;
         // fflush(stdout);

         pid = fork();

         // if(pid > 0){
         //    getrusage(RUSAGE_SELF, &usage_fmid);
         // }

         if(pid == 0) {

            // getrusage(RUSAGE_SELF, &usage_cbegin);

             int r = setitimer(ITIMER_PROF, &tick, NULL);
             __accmut__filter__mutants(from, to, i); // 筛选出当前包含的ID
             MUTATION_ID = eqclass[i].mut_id[0];

             // fprintf(stderr, "CHILD-> MUT: %d , PID: %d\n", MUTATION_ID, getpid());

            if (mprotect((void *)(&MUTATION_ID), PAGESIZE, PROT_READ)) {
                perror("COULD NOT mprotect !");
                exit(errno);
            }

             return eqclass[i].value; // 子进程返回子进程的计算结果
         } else {
             // fflush(stdout);
             waitpid(pid, NULL, 0);

            // fprintf(stderr, "FATHER-> MUT: %d , PID: %d\n", MUTATION_ID, getpid());

            #if 0

            int nptrs, j;
            void *buffer[100];
            char **strings;
            nptrs = backtrace(buffer, 20);

            fprintf(stderr, "backtrace() returned %d addresses\n", nptrs);

            strings = backtrace_symbols(buffer, nptrs);
            if (strings == NULL) {
                perror("backtrace_symbols");
                exit(EXIT_FAILURE);
            }
            for (j = 0; j < nptrs; j++)
                fprintf(stderr, "%s\n", strings[j]);

            free(strings);

            #endif
         }
    }

    __accmut__filter__mutants(from, to, 0); // 筛选出当前包含的ID
    return result; // 父进程返回父进程的计算结果
}

void __accmut__SIGSEGV__handler(){
    fprintf(stderr, "SIGSEGV !!! TID: %d, MID: %d , PID: %d\t\n", TEST_ID, MUTATION_ID, getpid());
    // signal(SIGSEGV, SIG_DFL);
    // exit(1);
    kill(getpid(), SIGKILL);
}

/** End Added **/

void __accmut__init(){

    __accmut__sepcific_timer();

    tick.it_value.tv_sec = VALUE_SEC;  // sec
    tick.it_value.tv_usec = VALUE_USEC; // u sec.
    tick.it_interval.tv_sec = INTTERVAL_SEC;
    tick.it_interval.tv_usec =  INTTERVAL_USEC;

	//perror("######### INIT START ####\n");
    
    signal(SIGPROF, __accmut__handler); 

    //signal(SIGSEGV, __accmut__SIGSEGV__handler);

    char path[256];
    strcpy(path, getenv("HOME"));
    strcat(path, "/tmp/accmut/mutations.txt");
	FILE *fp = fopen(path, "r");
	if(fp == NULL){
		fprintf(stderr, "FILE ERROR: mutation.txt CAN NOT OPEN !!! PATH: %s\n", path);
		exit(0);
	}
	int id;	
	char type[4];
	char buff[MUTFILELINE];	
	char tail[40];
	while(fgets(buff, MUTFILELINE, fp)){
		//fprintf(stderr, "%s", buff);
		sscanf(buff, "%d:%3s:%*[^:]:%*[^:]:%s", &id, type, tail);

		//fprintf(stderr, "%d -- %s --  %s\n", id, type, tail);
		Mutation* m = (Mutation *)malloc(sizeof(Mutation));
		if(!strcmp(type, "AOR")){
			m->type = AOR;
			int s_op, t_op;
			sscanf(tail, "%d:%d", &s_op, &t_op);
			m->op = s_op;
			m->t_op = t_op;

		}else if(!strcmp(type, "LOR")){
			m->type = LOR;
			int s_op, t_op;
			sscanf(tail, "%d:%d", &s_op, &t_op);
			m->op = s_op;
			m->t_op = t_op;
		}else if(!strcmp(type, "ROR")){
			m->type = ROR;
			int op, s_pre, t_pre;
			sscanf(tail, "%d:%d:%d", &op, &s_pre, &t_pre);
			m->op = op;
			m->s_pre = s_pre;
			m->t_pre = t_pre;
		}else if(!strcmp(type, "STD")){
			m->type = STD;
			int op, f_tp;
 			sscanf(tail, "%d:%d", &op, &f_tp);
			m->op = op;
			m->f_tp=f_tp;
		}else if(!strcmp(type, "LVR")){
			m->type = LVR;
			int op, op_i;
			long s_c, t_c;
			sscanf(tail, "%d:%d:%ld:%ld", &op, &op_i, &s_c, &t_c);
			m->op = op;
            m->t_op = op;
			m->op_index = op_i;
			m->s_con = s_c;
			m->t_con = t_c;
		}	
		ALLMUTS[id] = m;
	}

    int i;
    for(i = 0; i <= MAXMUTNUM; ++i) default_active_set[i] = 1; // 位表初始化

	//perror("######### INIT END ####\n");
}

//////

int __accmut__process_call_i32(){
    return 0;
}

long __accmut__process_call_i64(){
	return 0;
}

void __accmut__process_call_void(){

}

///////


int __accmut__process_i32_arith(int from, int to, int left, int right){


	int ori = __accmut__cal_i32_arith(ALLMUTS[to]->op , left, right);

    __accmut__filter__variant(from, to); // 首先筛选出表达式上的ID

    // generate recent_set
    int i;
    for(i = 0; i < recent_num; ++i) {
        if(recent_set[i] == 0) {
            temp_result[i] = ori;
        } else if (ALLMUTS[recent_set[i]]->type == LVR) {
            int op = ALLMUTS[recent_set[i]]->op;
            int t_con = ALLMUTS[recent_set[i]]->t_con;
            if(ALLMUTS[recent_set[i]]->op_index == 0) {
                temp_result[i] = __accmut__cal_i32_arith(op, t_con, right);
            } else {
                temp_result[i] = __accmut__cal_i32_arith(op, left, t_con);
            }
        } else {
            temp_result[i] = __accmut__cal_i32_arith(ALLMUTS[recent_set[i]]->t_op, left, right);
        }
    } // 计算结果
    if(recent_num == 1) { 
        // 如果只有一个ID，直接返回
        if(MUTATION_ID < from || MUTATION_ID > to) {
            return ori;
        }
        return temp_result[0];
    }

    // 否则划分等价类并进行fork
    /* divide */
    __accmut__divide__eqclass();
    /* fork */
    int result = __accmut__fork__eqclass(from, to);

    return result;

}

long __accmut__process_i64_arith(int from, int to, long left, long right){

	long ori = __accmut__cal_i64_arith(ALLMUTS[to]->op , left, right);

    __accmut__filter__variant(from, to);

    // generate recent_set
    int i;
    for(i = 0; i < recent_num; ++i) {
        if(recent_set[i] == 0) {
            temp_result[i] = ori;
        } else if (ALLMUTS[recent_set[i]]->type == LVR) {
            int op = ALLMUTS[recent_set[i]]->op;
            long t_con = ALLMUTS[recent_set[i]]->t_con;
            if(ALLMUTS[recent_set[i]]->op_index == 0) {
                temp_result[i] = __accmut__cal_i64_arith(op, t_con, right);
            } else {
                temp_result[i] = __accmut__cal_i64_arith(op, left, t_con);
            }
        } else {
            temp_result[i] = __accmut__cal_i64_arith(ALLMUTS[recent_set[i]]->t_op, left, right);
        }
    }

    if(recent_num == 1) {
        if(MUTATION_ID < from || MUTATION_ID > to) {
            return ori;
        }
        return temp_result[0];
    }
    /* divide */
    __accmut__divide__eqclass();
    /* fork */
    long result = __accmut__fork__eqclass(from, to);
    return result;
}


int __accmut__process_i32_cmp(int from, int to, int left, int right){
	int ori = __accmut__cal_i32_bool(ALLMUTS[to]->s_pre , left, right);

    __accmut__filter__variant(from, to);

    // generate recent_set
    int i;
    for(i = 0; i < recent_num; ++i) {
        if(recent_set[i] == 0) {
            temp_result[i] = ori;
        } else if (ALLMUTS[recent_set[i]]->type == LVR) {
            int pre = ALLMUTS[to]->s_pre;
            int t_con = ALLMUTS[recent_set[i]]->t_con;
            if(ALLMUTS[recent_set[i]]->op_index == 0) {
                temp_result[i] = __accmut__cal_i32_bool(pre, t_con, right);
            } else {
                temp_result[i] = __accmut__cal_i32_bool(pre, left, t_con);
            }
        } else {
            temp_result[i] = __accmut__cal_i32_bool(ALLMUTS[recent_set[i]]->t_pre, left, right);
        }
    }
    if(recent_num == 1) {
        if(MUTATION_ID < from || MUTATION_ID > to) {
            return ori;
        }
        return temp_result[0];
    }
    /* divide */
    __accmut__divide__eqclass();
    /* fork */
    //int result = ori;
    int result = __accmut__fork__eqclass(from, to);

    return result;
}

int __accmut__process_i64_cmp(int from, int to, long left, long right){
	long ori = __accmut__cal_i64_bool(ALLMUTS[to]->s_pre , left, right);

    __accmut__filter__variant(from, to);

    // generate recent_set
    int i;
    for(i = 0; i < recent_num; ++i) {
        if(recent_set[i] == 0) {
            temp_result[i] = ori;
        } else if (ALLMUTS[recent_set[i]]->type == LVR) {
            int pre = ALLMUTS[to]->s_pre;
            long t_con = ALLMUTS[recent_set[i]]->t_con;
            if(ALLMUTS[recent_set[i]]->op_index == 0) {
                temp_result[i] = __accmut__cal_i64_bool(pre, t_con, right);
            } else {
                temp_result[i] = __accmut__cal_i64_bool(pre, left, t_con);
            }
        } else {
            temp_result[i] = __accmut__cal_i64_bool(ALLMUTS[recent_set[i]]->t_pre, left, right);
        }
    }

    if(recent_num == 1) {
        if(MUTATION_ID < from || MUTATION_ID > to) {
            return ori;
        }
        return temp_result[0];
    }
    /* divide */
    __accmut__divide__eqclass();
    /* fork */
    long result = __accmut__fork__eqclass(from, to);
    return result;
}

void __accmut__process_st_i32(int from, int to, int *addr){
	int ori = ALLMUTS[to]->s_con;

    __accmut__filter__variant(from, to);

    // generate recent_set
    int i;
    for(i = 0; i < recent_num; ++i) {
        if(recent_set[i] == 0) {
            temp_result[i] = ori;
        } else {
            int t_con = ALLMUTS[recent_set[i]]->t_con;
            temp_result[i] = t_con;
        }
    }

    if(recent_num == 1) {
        if(MUTATION_ID < from || MUTATION_ID > to) {
            *addr = ori;
            return;
        }
        *addr = temp_result[0];
        return;
    }
    /* divide */
    __accmut__divide__eqclass();
    /* fork */
    int result = __accmut__fork__eqclass(from, to);
	//orig	
	*addr = result;
}

void __accmut__process_st_i64(int from, int to, long *addr){
	long ori = ALLMUTS[to]->s_con;

    __accmut__filter__variant(from, to);

    if(recent_num == 1) {
        return;
    }
    // generate recent_set
    int i;
    for(i = 0; i < recent_num; ++i) {
        if(recent_set[i] == 0) {
            temp_result[i] = ori;
        } else {
            long t_con = ALLMUTS[recent_set[i]]->t_con;
            temp_result[i] = t_con;
        }
    }

    /* divide */
    __accmut__divide__eqclass();
    /* fork */
    long result = __accmut__fork__eqclass(from, to);
	//orig	
	*addr = result;
}

#endif
