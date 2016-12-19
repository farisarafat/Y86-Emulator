#include "y86emul.h"

unsigned int insStrt;
unsigned int memSize;
unsigned int PC; //Program Counter
unsigned char* mem; //where all memory is stored
int registers[8]; //  where all the registers are stored
int f[3]; //flag array that holds ZF, OF, and SF
union converter uni; //4 bytes from memory to form a 32-bit integer
prgrmStatus status; //status holds the AOK, HLT, etc.


int main(int argc, char** argv){
        //check if valid argc
        if(argc != 2){
            fprintf(stderr, "%s", "ERROR, Arguments not at 2.\n");
            exit(EXIT_FAILURE);
        }

        //helper call
        if(argv[1][0] == '-' && argv[1][1] == 'h'){
            printf("Usage: ./y86emul <y86 input file>\n");
            return 0;
        }

        //opens file and see's if it exists
        FILE * inputfile = fopen(argv[1], "r");
        if(inputfile == NULL){
            fprintf(stderr, "%s", "ERROR, File not in directory.\n");
            exit(EXIT_FAILURE);
        }

        populateMem(inputfile);
        fetch();
        fclose(inputfile);

        if(status == ADR){
            printf("Invalid address found.\n");
        }
        else if(status == INS){
            printf("Invalid instruction found.\n");
        }

        return 0;
}

//populates the memory
void populateMem(FILE *inputfile){
    int rc;
    unsigned int addr;
    unsigned int data;
    char *direct;
    char temp [3];

    status = AOK;

        //mem size is determined by .size directive
        direct = malloc(sizeof(char)*8);
        fscanf(inputfile,"%s\t%x",direct,&memSize);
        mem = (unsigned char*)malloc(sizeof(unsigned char)*memSize);

        //loops through the rest of the directives and store them
        while(fscanf(inputfile, "%s", direct)!=EOF){

            if(!strcmp(direct, ".text")){
                fscanf(inputfile, "\t %x \t", &insStrt);
                PC = insStrt;
                //possible different function
                while(((rc = fgetc(inputfile)) != '\n')){
                    temp[0] = rc;
                    rc = fgetc(inputfile);
                    temp[1] = rc;
                    temp[2] = '\0';
                    sscanf(temp, "%x", &data);
                    mem[insStrt] = data;
                    insStrt++;
                }
            }
            else if(!strcmp(direct,".byte")){
                fscanf(inputfile, "\t %x \t %x",&addr, &data);
                mem[addr] = data;
            }
            else if(!strcmp(direct,".string")){
                fscanf(inputfile, "\t %x \t",&addr);
                rc = fgetc(inputfile);
                while((rc=getc(inputfile))!='\"'){
                    mem[addr] = rc;
                    addr++;
                }
            }
            else if(!strcmp(direct, ".long")){
                fscanf(inputfile, "\t %x \t %x", &addr, &data);
                store(addr, data);
            }
        }

    //frees malloced variables
    free(mem);
    free(direct);
}

void registerArith(int *rA, int *rB, int byte){
    *rB=byte%16;
    byte=byte/16;
    *rA=byte%16;
}
void store(int L, int number){//stores 4 bytes or 32-bit int stored in memory
    uni.integer = number;
    mem[L]= uni.byte[0];
    L++;
    mem[L]= uni.byte[1];
    L++;
    mem[L]= uni.byte[2];
    L++;
    mem[L]= uni.byte[3];
}
int readStored(int loc){ //returns 4 bytes or 32-bit int stored in memory
    uni.byte[0] = mem[loc];
    loc++;
    uni.byte[1] = mem[loc];
    loc++;
    uni.byte[2] = mem[loc];
    loc++;
    uni.byte[3] = mem[loc];

    return uni.integer;
}

//sets flags from previous opcode
void flagCheck(int number1, int number2, int prevRes, int opC){
    f[0] = 0; //ZF
    f[1] = 0; //OF
    f[2] = 0; //SF

    //zero check condition
    if(prevRes == 0){
        f[0] = 1;
    }
    //negative check
    else if(prevRes < 0){
        f[2] = 1;
    }
    //Overflow check
    else{
        if(opC == 0){
            if(((number1<0)==(number2<0) ) && ( (prevRes<0)!=(number1<0))){
                f[1] = 1;
            }
        }
        else if(opC == 4){
            if((prevRes/number2)!=number1){
                f[1] = 1;
            }
        }
        else if(opC == 1){
            if((number1<0 && number2>0 && prevRes < 0) || (number1 > 0 && number2 < 0 && prevRes > 0)){
                f[1] = 1;
            }
        }
    }

    if(opC == 2 || opC ==3){
        f[1] = 0;
    }
    if(opC == 12){
        f[1] = 0;
        f[2] = 0;
    }
}
int pop(){
    int number;
    number = readStored(registers[4]);
    registers[4] = registers[4]+4;
    return number;
}
int push(int number){
    registers[4] = registers[4] -4;
    store(registers[4], number);
    return 0;
}

//runs the instructions
void fetch(){
    int opC1;
    int opC2;
    int rA;
    int rB;
    int byte;

    if(PC+1<0 || PC+1>memSize){
        status = ADR;
    }

    while(1){
        //Split the Op codes
        byte = mem[PC];
        opC2 = byte%16;
        byte = byte/16;
        opC1 = byte%16;
        //nop
        if(opC1 == 0){
            PC++;
        }
        //HLT
        else if(opC1 == 1){
            status = HLT;
            printf("\nHLT Instruction encountered.\n");
            break;
        }
        //RRMOVL
        else if(opC1 == 2){
            rrmovl(rA, rB);
        }
        //IRMOVL
        else if(opC1 == 3){
            irmovl(rA, rB);
        }
        //RMMOVL
        else if(opC1 == 4){
            rmmovl(rA, rB);
        }
        //MRMOVL
        else if(opC1 == 5){
            mrmovl(rA, rB);
        }
        //OP1
        else if(opC1 == 6){
            op1(rA, rB, opC2);
        }
        else if(opC1 == 7){//JXX
            int res;
            res = readStored(PC+1);

            if(opC2 == 0){//JMP
                PC = res;
                continue;
            }
            else if(opC2 == 1){//JLE
                if((f[2]^f[1]) | f[0]){
                    PC = res;
                    continue;
                }
            }
            else if(opC2 == 2){//JL
                if((f[2]^f[1])){
                    PC = res;
                    continue;
                }
            }
            else if(opC2 == 3){//JE
                if((f[0])==1){
                    PC = res;
                    continue;
                }
            }
            else if(opC2 == 4){//JNE
                if((f[0])== 0){
                    PC = res;
                    continue;
                }
            }
            else if(opC2 ==5){//JGE
                if(~(f[2]^f[1])){
                    PC = res;
                    continue;
                }
            }
            else if(opC2 == 6){//JG
                if(~(f[2]^f[1]) & (~f[0])){
                    PC = res;
                    continue;
                }
            }
            PC+=5;
        }
        else if(opC1 == 8){//CALL
            if(push(PC+5)){
                printf("ERROR: Overflow");
                break;
            }
            PC = readStored(PC+1);
            continue;
        }
        else if(opC1 == 9){//RET
            PC = pop();
        }
        else if(opC1 == 10){//PUSHL
            registerArith(&rA, &rB, mem[PC+1]);

            if(push(registers[rA])){
                printf("ERROR: Overflow");
                break;
            }
            PC = PC +2;
        }
        else if(opC1 == 11){//POPL
            registerArith(&rA, &rB, mem[PC+1]);
            registers[rA] = pop();
            PC = PC + 2;
        }
        else if(opC1 == 12){//READX
            readx(rA, rB, opC2, byte);
        }
        else if(opC1 == 13){//WRITEX
            writex(rA, rB, opC2);
        }
        else if(opC1 == 14){//MOVSBL
            movsbl(rA, rB);
        }
        else{
            status = INS;
            break;
        }
    }
    return;
}

/*

All instruction code stored down here

*/

void rrmovl(int rA, int rB){
    registerArith(&rA, &rB, mem[PC+1]);
    registers[rB] = registers[rA];
    PC = PC + 2;
}

void irmovl(int rA, int rB){
    registerArith(&rA, &rB, mem[PC+1]);
    registers[rB] = readStored(PC + 2);
    PC = PC + 6;
}

void rmmovl(int rA, int rB){
    registerArith(&rA, &rB, mem[PC+1]);
    store(readStored(PC+2)+registers[rB], registers[rA]);
    PC = PC + 6;
}

void mrmovl(int rA, int rB){
    registerArith(&rA, &rB, mem[PC+1]);
    registers[rA] = readStored(readStored(PC + 2)+registers[rB]);
    PC = PC + 6;
}

void readx(int rA, int rB, int opC2, int byte){
    int displace;
    displace = readStored(PC+2);
    registerArith(&rA, &rB, mem[PC+1]);
    flagCheck(0,0,0,12);
    //READB
    if(opC2 == 0){
        int d;
        char RB;
        f[0] = 0;
        d = scanf("%c", &RB);
        if(d == EOF){
            f[0] = 1;
        }
        else{
            mem[registers[rA]+displace] = RB;
        }
    }
    //READL
    else if(opC2 == 1){
        int d;
        f[0] = 0;
        d = scanf("%d", &byte);
        if(d == EOF){
            f[0] = 1;
        }
        else{
            store(registers[rA]+displace, byte);
        }
    }
    PC = PC + 6;
}

void op1(int rA, int rB, int opC2){
    int res;
    registerArith(&rA, &rB, mem[PC+1]);

    if(opC2 == 0){//ADDL
        res=registers[rB]+registers[rA];
        flagCheck(registers[rB], registers[rA], res, opC2);
        registers[rB] = res;
    }
    else if(opC2 == 1){//SUBL
        res=registers[rB]-registers[rA];
        flagCheck(registers[rA], registers[rB], res, opC2);
        registers[rB] = res;
    }
    else if(opC2 == 2){//ANDL
        res=registers[rB]&registers[rA];
        flagCheck(registers[rB], registers[rA], res, opC2);
        registers[rB] = res;
    }
    else if(opC2 == 3){//XORL
        res = registers[rB]^registers[rA];
        flagCheck(registers[rB], registers[rA], res, opC2);
        registers[rB] = res;
    }
    else if(opC2 == 4){
        res = registers[rB]*registers[rA];
        flagCheck(registers[rB], registers[rA], res, opC2);
        if(f[1] == 1){
            printf("ERROR: Overflow\n");
        }
        registers[rB] = res;
    }
    PC+=2;
}

void writex(int rA, int rB, int opC2){
    int displace;
    displace = readStored(PC+2);
    registerArith(&rA, &rB, mem[PC+1]);
    //WRITEB
    if(opC2 == 0){
        printf("%c", mem[registers[rA]+displace]);
    }
    //WRITEL
    if(opC2 == 1){
        printf("%d", readStored(registers[rA]+displace));
    }
    PC += 6;
}

void movsbl(int rA, int rB){
    //the displacement for 32bit
    int displace;
    long res;
    char temp;

    displace = readStored(PC+2);
    registerArith(&rA,&rB,mem[PC+1]);
    temp = mem[registers[rB]+displace];
    res = (long)(temp - '0');
    store(registers[rA], res);

    flagCheck(0,0, temp, 14);
    PC = PC + 6;
}




