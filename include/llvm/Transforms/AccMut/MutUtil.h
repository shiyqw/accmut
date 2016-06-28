//===----------------------------------------------------------------------===//
//
// This file provides utils to load $HOME/tmp/accmut/mutations.txt to AllMutsMap
// 
// Add by Wang Bo. DEC 22, 2015
//
//===----------------------------------------------------------------------===//

#ifndef ACCMUT_MUT_UTIL_H
#define ACCMUT_MUT_UTIL_H

#include "llvm/Transforms/AccMut/Mutation.h"
#include "llvm/Transforms/AccMut/Config.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"

#include<map>
#include<vector>
#include<string>


using namespace std;
using namespace llvm;

class MutUtil{
public:
	static map<string, vector<Mutation*>* > AllMutsMap;
	static void getAllMutations();
	static void dumpAllMuts();
	static BasicBlock::iterator getLocation(Function &F, int instrumented_insts, int index);
private:
	static bool allMutsGeted;
	static Mutation * getMutation(string line);
};


#endif
