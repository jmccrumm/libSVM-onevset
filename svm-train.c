/* Updated libsvm svm-train 

 to support open set recognition based on

   W. J. Scheirer, A.  Rocha, A. Sapkota, T. E. Boult, "Toward Open Set Recognition," IEEE Transactions on Pattern Analysis and Machine Intelligence, vol. 35, no. 7, pp. 1757-1772, July, 2013    

@article{10.1109/TPAMI.2012.256,
author = {W. J. Scheirer and A. Rocha and A. Sapkota and T. E. Boult},
title = {Toward Open Set Recognition},
journal ={IEEE Transactions on Pattern Analysis and Machine Intelligence},
volume = {35},
number = {7},
issn = {0162-8828},
year = {2013},
pages = {1757-1772},
doi = {http://doi.ieeecomputersociety.org/10.1109/TPAMI.2012.256},
}


If you use any of the open set functions please cite appropriately.

There are also extensions using libMR which will be described in other
publications and should also cite based on libMR licensing if that is used as well.


These open set extensions to libSVM are subject to the following

Copyright (c) 2010-2013  Regents of the University of Colorado and Securics Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, subject to the following 3 conditions:

1) The above copyright notice and this permission notice shall be included in all
source code copies or substantial portions of the Software.

2) All documentation and/or advertising materials mentioning features or use of
this software must display the following acknowledgment:

      This product includes software developed in part at
      the University of Colorado Colorado Springs and Securics Inc.

3) Neither the name of Regents of the University of Colorado  and Securics Inc.  nor
 the names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.


THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


*/ 



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include "svm.h"
#define Malloc(type,n) (type *)malloc((unsigned long)(n)*sizeof(type))



struct svm_parameter param;		// set by parse_command_line
struct svm_problem prob;		// set by read_problem
struct svm_model *model;
struct svm_node *x_space;


void print_null(const char *s) {}


void exit_with_help()
{
	printf(
	"Usage: svm-train [options] training_set_file [model_file] \n"
	"options:\n"
	"-s svm_type : set type of SVM (default 0)\n"
	"	0 -- C-SVC\n"
	"	1 -- nu-SVC\n"
	"	2 -- one-class SVM\n"
	"	3 -- epsilon-SVR\n"
	"	4 -- nu-SVR\n"
	"	5 -- open-set oneclass SVM (open_set_training_file required)\n"
	"	6 -- open-set pair-wise SVM  (open_set_training_file required)\n"
	"	7 -- open-set binary SVM  (open_set_training_file required)\n"
	"	8 -- reserved for open-set WSVM (open_set_training_file required)\n"
	"	9 -- binary PIESVM (open_set_training_file required)\n"
	"	10 -- one-vs-all PIESVM (open_set_training_file required)\n"

	"-t kernel_type : set type of kernel function (default 2)\n"
	"	0 -- linear: u'*v\n"
	"	1 -- polynomial: (gamma*u'*v + coef0)^degree\n"
	"	2 -- radial basis function: exp(-gamma*|u-v|^2)\n"
	"	3 -- sigmoid: tanh(gamma*u'*v + coef0)\n"
	"	4 -- precomputed kernel (kernel values in training_set_file)\n"
	"-d degree : set degree in kernel function (default 3)\n"
	"-g gamma : set gamma in kernel function (default 1/num_features)\n"
	"-r coef0 : set coef0 in kernel function (default 0)\n"
	"-c cost : set the parameter C of C-SVC, epsilon-SVR, and nu-SVR (default 1)\n"
	"-n nu : set the parameter nu of nu-SVC, one-class SVM, and nu-SVR (default 0.5)\n"
	"-p epsilon : set the epsilon in loss function of epsilon-SVR (default 0.1)\n"
	"-m cachesize : set cache memory size in MB (default 100)\n"
	"-e epsilon : set tolerance of termination criterion (default 0.001)\n"
	"-h shrinking : whether to use the shrinking heuristics, 0 or 1 (default 1)\n"
	"-b probability_estimates : whether to train a SVC or SVR model for probability estimates, 0 or 1 (default 0)\n"
	"-wi weight : set the parameter C of class i to weight*C, for C-SVC (default 1)\n"
	"-v n: n-fold cross validation mode\n"
        "-B beta   will set the beta for fmeasure used in openset training, default =1\n"
        "-V filename   will log data about the opeset optimization process to filename\n"
        "-G nearpreasure farpressure   will adjust the pressures for openset optimiation. <0 will specalize, >0 will generalize\n"
        "-N  we build models for negative classes (used for multiclass where labels might be negative.  default is only positive models \n"
        "-E  do exaustive search for best openset (otherwise do the default greedy optimization) \n"
	"-q : quiet mode (no outputs)\n"
	);
	exit(1);
}

void exit_input_error(int line_num)
{
	fprintf(stderr,"Wrong input format at line %d\n", line_num);
	exit(1);
}

void parse_command_line(int argc, char **argv, char *input_file_name, char *model_file_name);
void read_problem(const char *filename);
void do_cross_validation(struct svm_problem &prob,const svm_parameter& param);



static char *line = NULL;
static int max_line_len;

static char* readline(FILE *input)
{
	int len;
	
	if(fgets(line,max_line_len,input) == NULL)
		return NULL;

	while(strrchr(line,'\n') == NULL)
	{
		max_line_len *= 2;
		line = (char *) realloc(line,(ulong) max_line_len);
		len = (int) strlen(line);
		if(fgets(line+len,max_line_len-len,input) == NULL)
			break;
	}
	return line;
}

int main(int argc, char **argv)
{
	char input_file_name[1024];
	char model_file_name[1024];
	const char *error_msg;
        //        struct svm_problem prob;		// set by read_problem
        //        struct svm_model *model;
        bool open_set = false;
        int cross_validation=0;


        param.optimize = OPT_BALANCEDRISK; //By default, optimize risk
        param.beta = 1.000; //Require classic fmeasure balance of recall and precision by default
        param.near_preasure = 0;
        param.far_preasure=0;
        param.rejectedID=-99999;
        param.openset_min_probability=.001;
        param.neg_labels=false;
        param.exaustive_open=false; /* do we do exaustive optimization for openset.. default is false */

	parse_command_line(argc, argv, input_file_name, model_file_name);

        if (param.svm_type == OPENSET_OC || param.svm_type == OPENSET_BIN || param.svm_type == OPENSET_PAIR || param.svm_type == OPEN_PIESVM || param.svm_type == PAIR_PIESVM || param.svm_type == ONE_VS_REST_PIESVM)
          {
            param.do_open = 1;
            open_set = true;
          }

	read_problem(input_file_name);
	error_msg = svm_check_parameter(&prob,&param);
        
	if(error_msg)
          {
            fprintf(stderr,"Error: %s\n",error_msg);
            exit(1);
          }
        
	if(cross_validation && !open_set) //Haven't implemented cross-validation for open set yet
          {
            do_cross_validation(prob,param);
          }
	else
        {
		    model = svm_train(&prob,&param);    	
		    if(svm_save_model(model_file_name,model))
		    {
		    	fprintf(stderr, "can't save model to file %s\n", model_file_name);
		    	exit(1);
		    }
		    svm_free_and_destroy_model(&model);
        }
	svm_destroy_param(&param);
	
	free(prob.y);
	free(prob.x);
	if(line) free(line);
        
	if(prob.labels) free(prob.labels);
	
        if(param.vfile != NULL) fclose (param.vfile);
	
	return 0;
}

void do_cross_validation(struct svm_problem &prob,const svm_parameter& param)
{
	int i;
	int total_correct = 0;
	double total_error = 0;
	double sumv = 0, sumy = 0, sumvv = 0, sumyy = 0, sumvy = 0;
	double *target = Malloc(double,(ulong) prob.l);

	svm_cross_validation(&prob,&param,param.nr_fold,target);
	if(param.svm_type == EPSILON_SVR || param.svm_type == OPENSET_OC || 
	   param.svm_type == NU_SVR)
          
	{
		for(i=0;i<prob.l;i++)
		{
			double y = prob.y[i];
			double v = target[i];
			total_error += (v-y)*(v-y);
			sumv += v;
			sumy += y;
			sumvv += v*v;
			sumyy += y*y;
			sumvy += v*y;
		}
		printf("Cross Validation Mean squared error = %g\n",total_error/prob.l);
		printf("Cross Validation Squared correlation coefficient = %g\n",
			((prob.l*sumvy-sumv*sumy)*(prob.l*sumvy-sumv*sumy))/
			((prob.l*sumvv-sumv*sumv)*(prob.l*sumyy-sumy*sumy))
			);
	}
	else
	{
		for(i=0;i<prob.l;i++)
			if(target[i] == prob.y[i])
				++total_correct;
		printf("Cross Validation Accuracy = %g%%\n",100.0*total_correct/prob.l);
	}
	free(target);
}

void parse_command_line(int argc, char **argv, char *input_file_name, char *model_file_name)
{
	int i;
	void (*print_func)(const char*) = NULL;	// default printing to stdout

	// default values
	param.svm_type = C_SVC;
	param.kernel_type = RBF;
	param.degree = 3;
	param.gamma = 0;	// 1/num_features
	param.coef0 = 0;
	param.nu = 0.5;
	param.cache_size = 100;
	param.C = 1;
	param.eps = 1e-3;
	param.p = 0.1;
	param.shrinking = 1;
	param.probability = 0;
	param.nr_weight = 0;
	param.weight_label = NULL;
	param.weight = NULL;
	param.cross_validation = 0;
	param.do_open = 0;

	// parse options
	for(i=1;i<argc;i++)
	{
		if(argv[i][0] != '-') break;
		if(++i>=argc)
			exit_with_help();
		switch(argv[i-1][1])
		{
			case 's':
				param.svm_type = atoi(argv[i]);
				break;
			case 't':
				param.kernel_type = atoi(argv[i]);
				break;
			case 'd':
				param.degree = atoi(argv[i]);
				break;
			case 'g':
				param.gamma = atof(argv[i]);
				break;
			case 'r':
				param.coef0 = atof(argv[i]);
				break;
			case 'n':
				param.nu = atof(argv[i]);
				break;
			case 'm':
				param.cache_size = atof(argv[i]);
				break;
			case 'c':
				param.C = atof(argv[i]);
				break;
			case 'e':
				param.eps = atof(argv[i]);
				break;
			case 'p':
				param.p = atof(argv[i]);
				break;
			case 'h':
				param.shrinking = atoi(argv[i]);
				break;
			case 'b':
				param.probability = atoi(argv[i]);
				break;
			case 'q':
				print_func = &print_null;
				i--;
				break;
			case 'v':
				param.cross_validation = 1;
				param.nr_fold = atoi(argv[i]);
				if(param.nr_fold < 2)
				{
					fprintf(stderr,"n-fold cross validation: n must >= 2\n");
					exit_with_help();
				}
				break;
			case 'w':
				++param.nr_weight;
				param.weight_label = (int *)realloc(param.weight_label,(ulong) (sizeof(int)*(ulong)param.nr_weight));
				param.weight = (double *)realloc(param.weight,(ulong) (sizeof(double)*(ulong)param.nr_weight));
				param.weight_label[param.nr_weight-1] = atoi(&argv[i-1][2]);
				param.weight[param.nr_weight-1] = atof(argv[i]);
				break;
                        case 'B':
                          param.beta = atof(argv[i]);
                          break;

                        case 'N':
                          param.neg_labels = true;
                          i--; // back up as we don't have arg
                          break;

                        case 'E':
                          param.exaustive_open = true;
                          i--; // back up as we don't have arg
                          break;

			case 'G':
				param.near_preasure = atof(argv[i++]);
				param.far_preasure = atof(argv[i]);
				break;
                        case 'V':
                          if(strlen(argv[i])>2) param.vfile = fopen(argv[i],"w");
                          if(param.vfile==NULL){
                            fprintf(stderr,"Verbose flag but could not open file %s, aborting!!!\n\n",argv[i]);
                            return;
                          }
                          break;


			default:
				fprintf(stderr,"Unknown option: -%c\n", argv[i-1][1]);
				exit_with_help();
		}
	}

	svm_set_print_string_function(print_func);

	// determine filenames

	if(i>=argc)
		exit_with_help();

	strcpy(input_file_name, argv[i]);

	if(i<argc-1)
		strcpy(model_file_name,argv[i+1]);
	else
	{
		char *p = strrchr(argv[i],'/');
		if(p==NULL)
			p = argv[i];
		else
			++p;
		sprintf(model_file_name,"%s.model",p);
	}
}

// read in a problem (in svmlight format)

void read_problem(const char *filename)
{
  int elements, max_index, inst_max_index, i, j;
  FILE *fp = fopen(filename,"r");
  char *endptr;
  char *idx, *val, *label;
  int max_label_count = 256;
  int nr_classes;
  
  double *lbl = (double *) malloc((ulong) (sizeof(double)*(ulong)max_label_count));
  
  if(fp == NULL)
    {
      fprintf(stderr,"can't open input file %s\n",filename);
      exit(1);
    }
  
  prob.l = 0;
  elements = 0;
  nr_classes=0;
  max_line_len = 10240;
  line = Malloc(char,(ulong) max_line_len);
  while(readline(fp)!=NULL)
    {
      char *p = strtok(line," \t"); // label
      if(*p == '\n' || *p == '#') continue;
      if (param.do_open && *p != '\n')
        {
          if (nr_classes >= max_label_count)
            {
              max_label_count *= 2;
              lbl = (double *)realloc(lbl, (ulong) sizeof(double)*(ulong)max_label_count);
            }
          
          lbl[nr_classes] = strtod(p,&endptr);
          if(endptr == p || *endptr != '\0')
            exit_input_error(nr_classes+1);
          
          for (int i = nr_classes - 1; i >= 0; i--)
            {
              if (lbl[nr_classes] == lbl[i])
                {
                  nr_classes--;
                  break;
                }
            }
          
          nr_classes++; //This is a hack. If the label already exists, we decrement
          //the number of classes. That way, when we're all done,
          //incrementing the number of classes will do The Right Thing.
        }
          
      // features
      while(1)
        {
          p = strtok(NULL," \t\n");
          if(p == NULL || *p == '\n') // check '\n' as ' ' may be after the last feature
            break;
          ++elements;
        }
      ++elements;
      ++prob.l;
    }
  rewind(fp);

  prob.y = Malloc(double,(ulong) prob.l);
  prob.x = Malloc(struct svm_node *,(ulong) prob.l);
  x_space = Malloc(struct svm_node,(ulong) elements);
  
  prob.nr_classes = nr_classes;
  if (param.do_open){
    prob.labels = Malloc(int, (ulong) prob.nr_classes);
    memcpy(prob.labels,lbl,prob.nr_classes);
    free(lbl);
  } else {
    prob.labels=NULL;
    free(lbl);
  }
  
  max_index = 0;
  j=0;
  for(i=0;i<prob.l;i++)
    {
      inst_max_index = -1; // strtol gives 0 if wrong format, and precomputed kernel has <index> start from 0
      readline(fp);
      prob.x[i] = &x_space[j];
      label = strtok(line," \t\n");
      if(label == NULL) // empty line
        exit_input_error(i+1);
      
      prob.y[i] = strtod(label,&endptr);
      if(endptr == label || *endptr != '\0')
        exit_input_error(i+1);
      
      while(1)
        {
          idx = strtok(NULL,":");
          val = strtok(NULL," \t");
          if(val == NULL)
            break;
          errno = 0;
          x_space[j].index = (int) strtol(idx,&endptr,10);
          if(endptr == idx || errno != 0 || *endptr != '\0' || x_space[j].index <= inst_max_index)
            exit_input_error(i+1);
          else
            inst_max_index = x_space[j].index;
          
          errno = 0;
          x_space[j].value = strtod(val,&endptr);
          if(endptr == val || errno != 0 || (*endptr != '\0' && !isspace(*endptr)))
            exit_input_error(i+1);
          
          ++j;
        }
      
      
      if(inst_max_index > max_index)
        max_index = inst_max_index;
      x_space[j++].index = -1;
      
    }

  if(param.gamma == 0 && max_index > 0)
    param.gamma = 1.0/max_index;
  if(param.kernel_type == PRECOMPUTED)
    for(i=0;i<prob.l;i++)
      {
        if (prob.x[i][0].index != 0)
          {
            fprintf(stderr,"Wrong input format: first column must be 0:sample_serial_number\n");
            exit(1);
          }
        if ((int)prob.x[i][0].value <= 0 || (int)prob.x[i][0].value > max_index)
          {
            fprintf(stderr,"Wrong input format: sample_serial_number out of range\n");
            exit(1);
          }
      }
  free(line); 
  line=NULL;
  fclose(fp);
}

//----------------------------------------------------------
// From svm-predict
//----------------------------------------------------------

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)

struct score_data
{
    double label;
    double score;
};


int compare_thresholds(const void *v1, const void *v2){
  double diff =(*(double*) v1) - (*(double*) v2);
  if(diff== 0) return 0;
  else     if(diff <  0) return -1;
  return 1;
}


int compare_scores(const void *v1, const void *v2){
  double diff =((struct score_data*) v1)->score - ((struct score_data*) v2)->score;
  if(diff== 0) return 0;
  else     if(diff <  0) return -1;
  return 1;
}





