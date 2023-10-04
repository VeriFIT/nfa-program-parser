#ifndef _MONA_MATA_H_
#define _MONA_MATA_H_

#include<stdio.h>
extern "C" {
#include <mona/bdd.h>
#include <mona/dfa.h>
#include <mona/mem.h>
}
#include<string>
#include<vector>
#include <iostream>
#include <fstream>
#include<tuple>
#include<cmath>
#include <filesystem>
#include <unordered_map>

unsigned sink_global;

// used in function mona_input and its subfunction
// cleared at each mona_input invocation
std::unordered_map<std::string,int> state_map;

// check that automaton is empty

int MonaDFA_check_empty(DFA *aut) {
	char *res;
	res=dfaMakeExample(aut, 1, 0, NULL);
	if (res==NULL) return 1; 
	else return 0;
}

//make union of a list of automata

// DFA *MonaDFA_product(std::vector<DFA*> operands, dfaProductType mode) {
// 	DFA *current=dfaCopy(operands[0]);
// 	for(int i=1;i< operands.size(); i++) {
// 		DFA *tmp;
// 		tmp=dfaProduct(current,operands[i],mode);
// 		dfaFree(current);
// 		current=dfaMinimize(tmp);
// 		dfaFree(tmp);
// 	}
// 	return current;

// }

DFA *MonaDFA_product(DFA* d1, DFA* d2, dfaProductType mode) {
	DFA *current=dfaCopy(d1);
    DFA *tmp;
	tmp=dfaProduct(current,d2,mode);
	dfaFree(current);
	current=dfaMinimize(tmp);
	dfaFree(tmp);
	return current;

}

// **********************************************
// basic operation for parsing the input string

// get a state number from the beginning of the line
int get_state (std::string line) {
	std::string f;
	int a;
	if((a=line.find(" "))!=std::string::npos) 
		f=line.substr(0,a);
	else f=line;
	if (f.length()==0) return -1;
	if ((f[0]=='%')||(f[0]=='@')) return -1;
	if (state_map.find(f)==state_map.end()) { 
		state_map[f]=state_map.size();
	}
	return state_map[f];
		
}
int get_neg_state (std::string line) {
	std::string f;
	int a;
	if((a=line.find(" "))!=std::string::npos) 
		f=line.substr(0,a);
	else f=line;
	if (f[0]!='!') return -1;
	if (f.length()<=1) return -1;
	std::string g=f.substr(1);
	if (state_map.find(g)==state_map.end()) { 
		state_map[g]=state_map.size();
	}
	return state_map[g];
}

// remove state from the beginning of the line.
std::string remove_state (std::string line) {
	int a;
	if((a=line.find(" "))==std::string::npos) return "";
	return line.substr(a+1);
}

int get_pos_indice (std::string line) {
	int a;
	std::string f;
	if((a=line.find(")"))!=std::string::npos) 
		f=line.substr(0,a);
	else if ((a=line.find(" "))!=std::string::npos)
		f=line.substr(0,a);
	     else f=line;
	if (f[0]!='a') return -1;
	     return std::stoi(f.substr(1));
}
int get_neg_indice (std::string line) {
	int a;
	std::string f;
	if((a=line.find(")"))!=std::string::npos) 
		f=line.substr(0,a);
	else if ((a=line.find(" "))!=std::string::npos)
		f=line.substr(0,a);
	     else f=line;
	if ((f[0]!='!')||(f[1]!='a')) return -1;
	     return std::stoi(f.substr(2));
}

std::string remove_indice (std::string line) {
	int a;
	if((a=line.find(" "))!=std::string::npos)	
		return line.substr(a+1);
	if((a=line.find(")"))!=std::string::npos)	
		return line.substr(a+1);
	return "";
	
}

int get_initial(std::string line) {
	if(((line.find("\%Initial"))!=std::string::npos)||((line.find("\%InitialFormula"))!=std::string::npos)) {
		int a=line.find(" ");
		std::string x=line.substr(a+1);
		return get_state(x);
	}
	return -1;

}

std::vector<int> get_finals(std::string line) {
	std::vector<int> finals;
	if(((line.find("\%Final"))!=std::string::npos)||((line.find("\%FinalFormula"))!=std::string::npos)) {
		int a=line.find(" ");
		std::string x=line.substr(a+1);
		while (1) {
			if (x.length()==0) break;
			if ((x[0]==' ')||(x[0]=='&')) {x=x.substr(1); continue;}
			if ((a=get_state(x))!=-1) {
				finals.push_back(a);
				x=remove_state(x);
			} else {
				std::cerr << "Error parsing final states: " << x << std::endl;
				exit(1);
			}
		}

	}
	return finals;
}


//**************************************************
// Creating BDDs for particular states

bdd_ptr create_nondet_tracks(int state, int sink, int nondet_track, int nondet_size, int seq, bdd_manager *bddm) {
	int zbytek= seq / 2;
	bdd_ptr rest_ptr;
	if (!(nondet_size))
		return bdd_find_leaf_hashed_add_root(bddm,state);
	rest_ptr=create_nondet_tracks(state,sink,nondet_track+1, nondet_size-1,zbytek, bddm);
	bdd_ptr sink_ptr=bdd_find_leaf_hashed_add_root(bddm,sink);
	bdd_ptr left, right;
	if (seq % 2) { left=sink_ptr; right=rest_ptr; }
	else { right=sink_ptr; left=rest_ptr; }

	return bdd_find_node_hashed_add_root(bddm,left,right,nondet_track);

}

// processing lines
bdd_ptr process_line(std::string s, int sink, bdd_manager *bddm,int nondet_track, int nondet_size, int seq) {
	// remove balast from the line
	if (s[0]=='(') return process_line(s.substr(1),sink,bddm,nondet_track, nondet_size,seq);
	if (s[0]==' ') return process_line(s.substr(1),sink,bddm,nondet_track, nondet_size,seq);
	if (s[0]=='&') return process_line(s.substr(1),sink,bddm,nondet_track, nondet_size,seq);
	int a;
	if((a=s.find("\\true"))!=std::string::npos)
		return process_line(s.substr(4),sink,bddm,nondet_track, nondet_size,seq);
	// create BDD
	if((a=s.find("\\false"))!=std::string::npos)
		return bdd_find_leaf_hashed_add_root(bddm,sink); // false -> send everything to SINK
	a=get_pos_indice(s);
	if (a!=-1) {
		return bdd_find_node_hashed_add_root(
				bddm, 
				bdd_find_leaf_hashed_add_root(bddm,sink),
				process_line(remove_indice(s),sink,bddm,nondet_track, nondet_size,seq),
				a);
	}
	a=get_neg_indice(s);
	if (a!=-1) {
		return bdd_find_node_hashed_add_root(
				bddm, 
				process_line(remove_indice(s),sink,bddm,nondet_track, nondet_size,seq),
				bdd_find_leaf_hashed_add_root(bddm,sink),
				a);
	}
	a=get_state(s);
	if (a!=-1) {
		return create_nondet_tracks(a,sink,nondet_track, nondet_size,seq,bddm); 
	}
	std::cout << "Input error\n";
	exit(1);


}

// process rules starting with a single state

unsigned prod_term_fn(unsigned  p, unsigned q) {    
	if (p==sink_global) return q;
	if (q==sink_global) return p;
	if (p==q) return p;
	std::cout << "Internal error in prod_term_fn\n";
	exit(1);
}


std::tuple<bdd_manager *, bdd_ptr> process_state(int state, int num, int sink, int first_nondet_track, int nondet_size, const std::unordered_map<int,std::vector<std::string>>& transitions) {
	int seq=0;
	sink_global=sink;
	bdd_manager *bddm;
	bdd_ptr state_ptr;
	for (const std::string& s : transitions.at(state)) {
		// Name of a state can not start with "a" and "!"
		if ((s[0]=='a')||(s[0]=='!')) {
			std::cout << "M2M Internal ERROR: name of a state starts with 'a' or '!' is not supported\n";
			exit(1);
		}
		int a=get_state(s);
		if (a!=state) continue;
		bdd_manager *tmp_bddm;
		tmp_bddm=bdd_new_manager(first_nondet_track+nondet_size+2,100);
		bdd_ptr tmp_state_ptr;
		tmp_state_ptr=process_line(remove_state(s), sink, tmp_bddm,first_nondet_track,nondet_size,seq);
		if (seq) {
			bdd_manager *tmp_bddm2;
			tmp_bddm2=bdd_new_manager(bdd_size(bddm)+bdd_size(tmp_bddm)+2,100);
  			bdd_make_cache(tmp_bddm2, bdd_size(bddm)+bdd_size(tmp_bddm)+2, 100);    
  			tmp_bddm2->cache_erase_on_doubling = TRUE ;
			state_ptr=bdd_apply2_hashed(bddm, state_ptr, tmp_bddm,tmp_state_ptr, tmp_bddm2,&prod_term_fn);
			bdd_kill_manager(bddm);
			bdd_kill_manager(tmp_bddm);
			bddm=tmp_bddm2;

		} else {
			bddm=tmp_bddm;
			state_ptr=tmp_state_ptr;
		}
		seq++;
		
	}
	return std::make_tuple(bddm, state_ptr);
}

DFA *NewDFA(unsigned n, unsigned init, bdd_manager *bddm) {
  DFA *a;
  a = (DFA *) mem_alloc(sizeof *a);
  a->bddm = bddm;
  a->ns = n;
  a->q = (bdd_ptr *) mem_alloc((sizeof *(a->q)) * n);
  a->f = (int *) mem_alloc((sizeof *(a->f)) * n); 
  a->s = init;
  return a;
}



DFA *mona_input (std::filesystem::path filename) {
	state_map.clear();
	// count the number of transitions starting from particular states
	std::vector<int> aut_states, finals;
	int maxindice=0;
	int initstate=-1;

	std::unordered_map<int,std::vector<std::string>> transitions;

	std::ifstream infile(filename);
	if (!infile.is_open()) {
		std::cout << "ERROR: Can not open file: "<< filename << std::endl;
		exit(1);
	}
	std::string line;
	while (std::getline(infile, line)) {
		std::string s = line;
		int a;
		if ((a=get_initial(s))!=-1) {
			if (initstate!=-1) {
				std::cout << "Error: multiple definitions of init states\n";
				exit(1);
			}
			initstate=a;
			if (aut_states.size()<=a)
				aut_states.resize(a+1);
		}
		if (finals.size()==0) finals=get_finals(s);
		a=get_state(s);
		if (a==-1) continue;
		transitions[a].push_back(line);
		if (aut_states.size()<=a)
			aut_states.resize(a+1);
		aut_states[a]++;
		//get the biggest indice from the line
		std::string str=remove_state(s);
		int i1,i2;
		while (1) {
			if (str.size()==0) break;
			if (str[0]=='(') { str=str.substr(1); continue;}
			if (str[0]==' ') { str=str.substr(1); continue;}
			if (str[0]=='&') { str=str.substr(1); continue;}
			/* remove true from the input */
			if((str.find("\\true"))==0) {str=str.substr(4); continue; } 
			if (((i1=get_pos_indice(str))!=-1) || ((i2=get_neg_indice(str))!=-1)) {
				if (i1>maxindice) maxindice=i1;
				if (i2>maxindice) maxindice=i2;
				str=remove_indice(str);
			} else {
				/* get a state from the EOF for the case that the state is unused on a LHS of a rule. */
				int b=get_state(str);
				if (aut_states.size()<=b)
					aut_states.resize(b+1);
				break;
			}

		}
	}
	infile.close();

	// SINK is a fresh state with higher number then all other states in the automaton
	if (state_map.find("@SINK")!=state_map.end()) { 
		std::cout << "M2M internal error: @SINK is reserved key for a sink state\n";
		exit(1);
	}
	state_map["@SINK"]=state_map.size();
	int sink= state_map["@SINK"];
	// we count, how many bits we need to simulate nondeterminism by a deterministic automata
	int nondet_level=0;
	for (int i : aut_states) {
		if (i>nondet_level) nondet_level=i;
	}
	int level_bits=(int)log2(nondet_level)+1;
	// Here try to compute size of BDD to avoid reallocations.
	// ((number_of_states * nondet_level) * ( maxindice + level_bits +1))
	//    -- number of rules --           * -- lenght of a rule --
	bdd_manager *bddm;
	bddm=bdd_new_manager((((sink+1) * nondet_level) * ( maxindice + level_bits +1)),1000);
	DFA *aut;
	aut=NewDFA(sink+1,initstate,bddm);
	bdd_prepare_apply1(bddm);
	for (int i=0; i<sink; i++) {
		bdd_ptr state_ptr;
		if (aut_states[i]) {
			// there are defined rules for state i
			bdd_manager *state_bddm;
			std::tie(state_bddm, state_ptr)=process_state(i,aut_states[i], sink, maxindice+1, level_bits, transitions);
			state_ptr=bdd_apply1(state_bddm, state_ptr, bddm, &fn_identity);
			bdd_kill_manager(state_bddm);
		} else {
			// there are no defined rules for state i
			state_ptr=bdd_find_leaf_hashed_add_root(bddm,sink);
		}
		(aut->q)[i]=state_ptr;
		(aut->f)[i]=-1;
	}
	for (int i : finals) {
		(aut->f)[i]=1;
	}
	// set sink
	(aut->q)[sink]=bdd_find_leaf_hashed_add_root(bddm,sink);
	(aut->f)[sink]=-1;
	for (int i=maxindice+level_bits;i>maxindice;i--) {
		DFA *aut_tmp;
		aut_tmp=dfaProject(aut,i);
		dfaFree(aut);
		aut=dfaMinimize(aut_tmp);
		dfaFree(aut_tmp);
	}
	return aut;

}

#endif
