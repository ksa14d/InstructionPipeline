/*************************************************************
 *                                                           *
 *                                                           *
 *                                                           *
 *  Name: Karthik Shankar Achalkar(ksa14d)                   *
 *                                                           *
 *                                                           *
 *  Class: CDA5155                                           *
 *                                                           *
 *  Assignment: Implementing MIPS Pipelines instruction      *
 *                                                           *
 *                                                           *
 *  Compile: "make"                                          *
 *                                                           *
 * 							     *
 *							     *
 *************************************************************/
#include<iostream>
#include<fstream>
#include<climits>
#include<stdio.h>
#include<string>
#include<string.h>
#include<math.h>
using namespace std;
int  cycles_sa = 0;
int  cycles_mul = 0;
int  cycles_div = 0;
class Stats
{
public:
    double total_cycles;
    double total_stall_cycles;

    double load_cycles;
    double structural_cycles;
    double data_cycles;

    double waw_squash;
    double branch_flush;

    double load_percent_stalls;
    double structural_percent_stalls;
    double data_percent_stalls;
    double stall_percent;

    double load_percent_total;
    double structural_percent_total;
    double data_percent_total;
    double all_percent;

    Stats()
    {
        load_cycles = structural_cycles = data_cycles = 0;
        load_percent_stalls = structural_percent_stalls = data_percent_stalls = 0;
        load_percent_total = structural_percent_total = data_percent_total = 0;
        waw_squash = branch_flush = 0;
    }

    void ComputeRatio()
    {
        if(total_stall_cycles !=0)
        {
            load_percent_stalls = load_cycles/total_stall_cycles*100;
            structural_percent_stalls = structural_cycles / total_stall_cycles*100;
            data_percent_stalls = data_cycles / total_stall_cycles*100;
        }
        stall_percent = load_percent_stalls+structural_percent_stalls+data_percent_stalls;


        if(total_stall_cycles !=0)
        {
            load_percent_total = load_cycles/ total_cycles*100;
            structural_percent_total = structural_cycles/total_cycles*100;
            data_percent_total =  data_cycles / total_cycles*100;
        }
        all_percent = load_percent_total + structural_percent_total + data_percent_total;
    }
    void PrintInfo()
    {
        printf("\nhazard type  cycles  %% of stalls  %% of total\n");
        printf("-----------  ------  -----------  ----------\n");
        printf("load-delay   %6.0lf       %6.2lf      %6.2lf\n",load_cycles,load_percent_stalls,load_percent_total);
        printf("structural   %6.0lf       %6.2lf      %6.2lf\n",structural_cycles,structural_percent_stalls,structural_percent_total);
        printf("data         %6.0lf       %6.2lf      %6.2lf\n",data_cycles,data_percent_stalls,data_percent_total);
        printf("-----------  ------  -----------  ----------\n");
        printf("total        %6.0lf       %6.2lf      %6.2lf\n",total_stall_cycles,stall_percent,all_percent);

        printf("\nWAW squashes: %.0lf\n",waw_squash);
        printf("branch flushes: %.0lf\n",branch_flush);
    }

};
Stats *stats = new Stats();

class Cycles
{
public:
    Cycles(int n)
    {
        num = n;
        IsWAWSquash = false;
        IsBranchFlush = false;
    }
    int num;
    bool IsWAWSquash;
    bool IsBranchFlush;
    string IF;
    string ID;
    string EX;
    string MEM;
    string WB;
    string FADD;
    string FMUL;
    string FDIV;
    string FWB;
};

class Register
{
public:
    char RF;
    int i;
    Register()
    {

    }
};

int CurrFPAddInst = -1;
int CurrFPMulInst = -1;
int CurrFPDivInst = -1;
class Instruction
{
    string strInst ;
    int offset;
public:
    int ReservedFWBCycle ;
    bool IsWriteBackDependent;
    bool IsLast;
    bool IsFPAdd;
    bool IsFPMul;
    bool IsFPDiv;
    bool IsFP;
    int cycle_exec;
    char predict;
    bool IsFlushed;
    bool DoesWriteBack;
    bool IsSquashed;
    string opcode;
    int inst_num;
    bool IsProcessed;
    Register SrcReg[2];
    int nsrc;
    Register DstReg;
    bool IsStalled;
    int currentState;
    bool IsDataDependent;
    bool IsLoadDependent;
    bool IsFirstRegTarget;
    bool IsBranch ;
    Instruction(int inum , string line)
    {
        // 0 -> take it to IF
        // 1 -> to ID
        // 2 -> to EX
        // 3 -> to MEM
        // 4 -> to WB
        ReservedFWBCycle = -1;
        IsWriteBackDependent = false;
        IsSquashed = false;
        IsFlushed = false;
        IsLoadDependent=false;
        IsBranch = false;
        char op[200];
        Register R[3];
        char junk[1000];
        if(sscanf(line.c_str(),"%s\t%c%d,%d(R%d)",op,&R[0].RF,&R[0].i,&offset,&R[1].i)==5)
        {
            nsrc = 1;
            if(strcmp(op,"LW")==0 || strcmp(op,"L.S")==0)
            {
                IsFirstRegTarget = true;
                DstReg.RF = R[0].RF;
                DstReg.i = R[0].i;
                SrcReg[0].RF = 'R';
                SrcReg[0].i = R[1].i;
                sprintf(junk,"%s\t%c%d,%d(%c%d)",op,DstReg.RF,DstReg.i,offset,SrcReg[0].RF,SrcReg[0].i) ;
            }
            else
            {
                IsFirstRegTarget = false;
                SrcReg[0].RF = R[0].RF;
                SrcReg[0].i = R[0].i;
                DstReg.RF = 'R';
                DstReg.i = R[1].i;
                sprintf(junk,"%s\t%c%d,%d(%c%d)",op,SrcReg[0].RF,SrcReg[0].i,offset,DstReg.RF,DstReg.i) ;
            }
        }
        else if(sscanf(line.c_str(),"%s\t%c%d,%c%d,%c%d:%c",op,&SrcReg[0].RF,&SrcReg[0].i,&SrcReg[1].RF,&SrcReg[1].i,&R[2].RF,&R[2].i,&predict)==8)
        {
            nsrc = 2;
            IsBranch = true; // both are not target
            sprintf(junk,"%s\t%c%d,%c%d,%c%d:%c",op,SrcReg[0].RF,SrcReg[0].i,SrcReg[1].RF,SrcReg[1].i,R[2].RF,R[2].i,predict) ;
        }
        else if(sscanf(line.c_str(),"%s\t%c%d,%c%d,%c%d",op,&R[0].RF,&R[0].i,&R[1].RF,&R[1].i,&R[2].RF,&R[2].i)==7)
        {
            nsrc = 2;
            IsFirstRegTarget = true;
            SrcReg[0].RF = R[1].RF;
            SrcReg[0].i= R[1].i;
            SrcReg[1].RF = R[2].RF;
            SrcReg[1].i= R[2].i;
            DstReg.RF = R[0].RF;
            DstReg.i = R[0].i;
            sprintf(junk,"%s\t%c%d,%c%d,%c%d",op,DstReg.RF,DstReg.i,SrcReg[0].RF,SrcReg[0].i,SrcReg[1].RF,SrcReg[1].i);
        }
        else if(sscanf(line.c_str(),"%s\t%c%d,%c%d",op,&R[0].RF,&R[0].i,&R[1].RF,&R[1].i)==5)
        {
            nsrc = 1;
            IsFirstRegTarget = true;
            DstReg.RF = R[0].RF;
            DstReg.i = R[0].i;
            SrcReg[0].RF = R[1].RF;
            SrcReg[0].i = R[1].i;
            sprintf(junk,"%s\t%c%d,%c%d",op,DstReg.RF,DstReg.i,SrcReg[0].RF,SrcReg[0].i) ;
            /*  if(strcmp(op,"MTC1")==0)
            {
                IsFirstRegTarget = false;
                SrcReg[0].RF = R[0].RF;SrcReg[0].i = R[0].i;
                DstReg.RF = R[1].RF; DstReg.i = R[1].i;
                      sprintf(junk,"%s\t%c%d,%c%d",op,SrcReg[0].RF,SrcReg[0].i,DstReg.RF,DstReg.i) ;
             } */
        }
        strInst = line;
        printf(" %2d. %s\n",inum,strInst.c_str());
        opcode = op;
        if(IsBranch || opcode.compare("SW")==0 || opcode.compare("S.S")==0)
        {
            DoesWriteBack = false;
        }
        else
        {
            DoesWriteBack = true;
        }
        IsProcessed = false;
        IsFP = IsFPAdd = IsFPMul = IsFPDiv = false;
        currentState = 0;
        inst_num = inum;
        if(line.find(".S")!= string::npos)IsFP = true;
        if(opcode.compare("ADD.S")== 0 || opcode.compare("SUB.S")== 0)
        {
            cycle_exec = cycles_sa;
            IsFPAdd = true;
        }
        else if(opcode.compare("MUL.S")== 0)
        {
            cycle_exec = cycles_mul;
            IsFPMul = true;
        }
        else if(opcode.compare("DIV.S")==0)
        {
            cycle_exec = cycles_div;
            IsFPDiv = true;
        }
        else cycle_exec = 1;
    }

    Cycles* currentCycle = NULL;
    void Process(Cycles* tick)
    {
        char ch;
        cin>>ch;
        currentCycle = tick;
        switch(currentState)
        {
        case 0:
            Fetch();
            break;
        case 1:
            Decode();
            break;
        case 2:
            Execute();
            break;
        case 3:
            MEM();
            break;
        case 4:
            WriteBack();
            currentState++;
            break;
        }
        if(currentState ==5)IsProcessed = true;
    }

    void Fetch()
    {
        currentCycle->IF = to_string(inst_num);
        currentState++;
        if(IsFlushed)currentState = 5;
    }
    void Decode()
    {
        currentCycle->ID = to_string(inst_num);
        currentState++;
    }
    void stall()
    {
        IsStalled = true;
        stats->total_stall_cycles++;
        currentCycle->ID = "stall";
        if(!IsLast)
            currentCycle->IF = "stall";
    }
    void Execute()
    {
        if(IsWriteBackDependent)// structural dependency given highest priority
        {
            stall();
            stats->structural_cycles++;
            return;
        }
        if (IsLoadDependent)//  Load Dependency
        {
            stall();
            stats->load_cycles++;
            return;
        }
        if(IsFPAdd)
        {
            if(currentCycle->FADD.compare("")!=0 ||(CurrFPAddInst!=-1&&CurrFPAddInst!=inst_num)) // structural first
            {
                stall();
                stats->structural_cycles++;
                return;
            }
            else if(IsDataDependent)// data next
            {
                stall();
                stats->data_cycles++;
                return;
            }
            else if(CurrFPAddInst == -1)
            {
                IsStalled = false;
                CurrFPAddInst = inst_num;
            }


            currentCycle->FADD = to_string(inst_num);
            cycle_exec--;
            if(cycle_exec == 0)CurrFPAddInst = -1;
        }
        else if(IsFPMul)
        {
            if(currentCycle->FMUL.compare("")!=0 ||(CurrFPMulInst!=-1 && CurrFPMulInst!=inst_num))
            {
                stall();
                stats->structural_cycles++;
                return;
            }
            else if(IsDataDependent)
            {
                stall();
                stats->data_cycles++;
                return;
            }
            else if(CurrFPMulInst == -1)
            {
                IsStalled = false;
                CurrFPMulInst = inst_num;
            }
            currentCycle->FMUL = to_string(inst_num);
            cycle_exec--;
            if(cycle_exec == 0)CurrFPMulInst = -1;
        }
        else if(IsFPDiv)
        {
            if(currentCycle->FDIV.compare("")!=0 ||(CurrFPDivInst!=-1&&CurrFPDivInst!=inst_num))
            {
                stall();
                stats->structural_cycles++;
                return;
            }
            else if(IsDataDependent)
            {
                stall();
                stats->data_cycles++;
                return;
            }
            else if(CurrFPDivInst == -1)
            {
                IsStalled = false;
                CurrFPDivInst = inst_num;
            }
            CurrFPDivInst = inst_num;
            currentCycle->FDIV = to_string(inst_num);
            cycle_exec--;
            if(cycle_exec == 0)CurrFPDivInst = -1;
        }
        else if(IsFP )
        {
            if(IsDataDependent)   // other FPdata dependency data
            {
                stall();
                stats->data_cycles++;
                return;
            }
            IsDataDependent = false;
            IsStalled = false;
            currentCycle->EX = to_string(inst_num);
            cycle_exec--;
        }
        else if(IsBranch)
        {
            if(IsDataDependent)   // branch dependency data
            {
                stall();
                stats->data_cycles++;
                return;
            }
            IsStalled = false;
            currentState=5;
        }
        else
        {

            IsDataDependent = false;
            IsStalled = false;
            currentCycle->EX = to_string(inst_num);
            cycle_exec--;
        }
        if(cycle_exec == 0)
        {
            if(IsFP && opcode.find("CVT") != 0 && opcode.compare("S.S")!= 0 && opcode.compare("MOV.S") && opcode.compare("L.S")!=0)// check for other cvt
            {
                currentState=4;
            }
            else
            {
                currentState++;
            }
        }
    }
    void MEM()
    {
        currentCycle->MEM = to_string(inst_num);
        currentState++;
    }
    void WriteBack()
    {
        if(IsSquashed)return;
        if((IsFP || opcode.compare("MTC1")==0) && DoesWriteBack)
        {
            currentCycle->FWB = to_string(inst_num);
        }
        else if(DoesWriteBack)
        {
            currentCycle->WB = to_string(inst_num);
        }
    }
};


class CPU
{
public:
    CPU()
    {
        n_iqueue = 0;
    }
    bool HasPendingInstructions;
    Cycles** CPUCycles = new Cycles*[200];
    Instruction** iqueue = new Instruction*[200]; // in order queue
    int n_cycles;
    int n_iqueue;


    bool CheckDataDependency(int index)
    {
        Instruction* inst = iqueue[index];
        inst->IsDataDependent = false;
        inst->IsLoadDependent = false;
        // get first instance of the previous instruction in the que which is not processed and have inst.src reg === previnst.dest reg
        for(int i = 0; i < inst->nsrc; i++)
        {
            for(int j = index-1 ; j >=0 ; j--)
            {
                if(inst->SrcReg[i].RF == iqueue[j]->DstReg.RF && inst->SrcReg[i].i == iqueue[j]->DstReg.i)
                {
                    if(!iqueue[j]->IsProcessed)
                    {
                        if(iqueue[j]->opcode.compare("LW")==0||iqueue[j]->opcode.compare("L.S")==0)// load dependency
                            inst->IsLoadDependent = true;
                        else if((inst->opcode.compare("S.S")==0 && iqueue[j]->cycle_exec > 0) || inst->IsBranch) // other data dependency
                            inst->IsDataDependent = true;
                        else if((inst->opcode.compare("S.S")!=0 && iqueue[j]->cycle_exec >= 0) || inst->IsBranch) // other data dependency
                            inst->IsDataDependent = true;
                        return true;
                    }
                    break;
                }
            }
        }
        return false;
    }

    void ReserveFWB(int index, int cycle_num)
    {
        Instruction* inst = iqueue[index];
        inst->IsWriteBackDependent= false;
        int FWBCycle = -1;
        if(inst->IsFPAdd ||inst->IsFPMul || inst->IsFPDiv)
            FWBCycle = cycle_num + inst->cycle_exec + 1;
        else
            FWBCycle = cycle_num + 2 + 1; // EX MEM WB

        bool IsFWBCycleValid = true;
        for(int j = index-1 ; j >=0 ; j--)
        {
            if(iqueue[j]->DoesWriteBack && iqueue[j]->IsFP )
            {
                if(iqueue[j]->ReservedFWBCycle == FWBCycle && !iqueue[j]->IsSquashed)// collision
                {
                    IsFWBCycleValid =false;
                    break;
                }
            }
        }
        if(IsFWBCycleValid)// reserve it
        {
            inst->ReservedFWBCycle = FWBCycle;
        }
        else
        {
            inst->IsWriteBackDependent = true;
        }
    }

    void Squash(int index)
    {
        Instruction* inst = iqueue[index];

        for(int j = index-1 ; j >=0 ; j--)
        {
            if(!iqueue[j]->IsSquashed && !iqueue[j]->IsProcessed && iqueue[j]->DoesWriteBack && iqueue[j]->DstReg.RF == inst->DstReg.RF && iqueue[j]->DstReg.i == inst->DstReg.i )
            {
                if(iqueue[j]->currentState < 4)//  make < only to allow fwb stalls
                {
                    iqueue[j]->IsSquashed = true;
                    stats->waw_squash++;
                }
            }
        }
    }

    void Clock()
    {
        Cycles* ClkCycle = new Cycles(n_cycles+1);
        HasPendingInstructions = false;
        for(int i = 0 ; i < n_iqueue ; i++) // check for hazards here
        {
            Instruction* inst = iqueue[i];
            if(!inst->IsProcessed)
            {
                if(inst->IsBranch && inst->predict == 'T' && i+1 < n_iqueue)
                {
                    if(!iqueue[i+1]->IsFlushed)
                    {
                        iqueue[i+1]->IsFlushed = true;
                        stats->branch_flush++;
                    }
                }
                if(inst->currentState == 2)
                {

                    if(inst->DoesWriteBack && inst->IsFP && !inst->IsFlushed )
                    {
                        ReserveFWB(i,ClkCycle->num);
                    }
                    CheckDataDependency(i);
                }
                inst->Process(ClkCycle);
                if(!inst->IsProcessed)
                    HasPendingInstructions = true;
                if(inst->DoesWriteBack && inst->currentState == 4)// this inst wants to wb
                {
                    // if this instruction does a write back complete search for all previous unprocessed instruction which write back to the same register and squash them
                    Squash(i);
                }
                if(inst->IsDataDependent || inst->IsLoadDependent || inst->IsStalled || ClkCycle->IF.compare("")!=0)break; // dont clock upcoming instruction let them be in the same state
            }
        }
        if(ClkCycle->IF.compare("")==0 && ClkCycle->ID.compare("")==0 && ClkCycle->EX.compare("")==0 && ClkCycle->MEM.compare("")==0 && ClkCycle->WB.compare("")==0 && ClkCycle->FADD.compare("")==0 && ClkCycle->FMUL.compare("")==0 && ClkCycle->FDIV.compare("")==0 && ClkCycle->FWB.compare("")==0 )
        {
            n_cycles--;
        }
        else
        {
            printf("%5d %5s %5s %5s %5s %5s %5s %5s %5s %5s\n",ClkCycle->num,ClkCycle->IF.c_str(),ClkCycle->ID.c_str(),ClkCycle->EX.c_str(),ClkCycle->MEM.c_str(),ClkCycle->WB.c_str(),ClkCycle->FADD.c_str(),ClkCycle->FMUL.c_str(),ClkCycle->FDIV.c_str(),ClkCycle->FWB.c_str());
        }
        CPUCycles[n_cycles++] = ClkCycle;
        stats->total_cycles = n_cycles;
    }

    void AddInstruction(Instruction* Inst)
    {
        iqueue[n_iqueue++] = Inst ;
    }
};

int main(int argc, char** argv)
{
    ifstream configFile("config.txt");
    string line;
    int n_inst = 0;
    CPU* Processor = new CPU();
    Instruction** inst = new Instruction*[200];
    char s[200];
    while( getline( configFile, line ) )
    {
        if(line.find("fp_add")!= string::npos)
        {
            sscanf(line.c_str(),"%[^:]:%d",s,&cycles_sa);
        }
        else if(line.find("fp_mul")!= string::npos)
        {
            sscanf(line.c_str(),"%[^:]:%d",s,&cycles_mul);
        }
        else if(line.find("fp_div")!= string::npos)
        {
            sscanf(line.c_str(),"%[^:]:%d",s,&cycles_div);
        }
    }

    printf("Configuration:\n");
    printf("   fp adds and subs cycles: %2d\n",cycles_sa);
    printf("      fp multiplies cycles: %2d\n",cycles_mul);
    printf("         fp divides cycles: %2d\n",cycles_div);

    printf("\n\nInstructions:\n");
    int i = 0;
    while( getline( cin, line ) )
    {
        inst[n_inst++]  = new Instruction(++i,line);
    }
    printf("\n\ncycle    IF    ID    EX   MEM    WB  FADD  FMUL  FDIV   FWB\n");
    printf("----- ----- ----- ----- ----- ----- ----- ----- ----- -----\n");
    for(int i = 0 ; i< n_inst ; i++)
    {
        if(i == n_inst-1)inst[i]->IsLast = true;
        Processor->AddInstruction(inst[i]);
        Processor->Clock();
    }
    while(Processor->HasPendingInstructions)
    {
        Processor->Clock();
    }
    stats->ComputeRatio();
    stats->PrintInfo();

    return 0;

}
