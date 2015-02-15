
#include <stdio.h>
#include <glpk.h>
#include "solve_lp_glpk.h"
#include "QP.h"

//#define DEBUG_GLPK
void simplify(QP* qp)
{
	// simplify
	bool keep_running = true; 
	int iter=0; 
	int keep = 0; 
	while (keep_running)
	{
		iter++; 
		keep_running = false; 
		int num_constraints = qp->b.size(); 
		int discard = 0; 
		keep=0; 
		int delete_coef = 0; 
		for(int i=0;i<num_constraints;i++)
		{
			vector<int>* idx = NULL;
			vector<float>* coef = NULL;
			int ret = qp->A.get(&idx, &coef, i);
			assert(idx!=NULL);
			assert(coef!=NULL);
			assert(ret>0);
			assert(idx->size()==coef->size());

			for (int j=0; j<idx->size();j++)
			{
				assert(idx->at(j)<qp->lb.size()); 
				if (qp->lb[idx->at(j)]==qp->ub[idx->at(j)])
				{
					// variable is fixed
					qp->b[i]-=coef->at(j); 
					idx->erase(idx->begin()+j); 
					coef->erase(coef->begin()+j); 
					delete_coef++; 
					keep_running = true;
					break; 
				}
			}

			if (idx->size()==1 && qp->eq_idx[i]>0.5)// equality constraints
			{
				assert(qp->lb[idx->at(0)]*coef->at(0) <= qp->b[i]+0.001); 
				assert(qp->ub[idx->at(0)]*coef->at(0) >= qp->b[i]-0.001); 

				qp->lb[idx->at(0)] = qp->b[i]/coef->at(0); 
				qp->ub[idx->at(0)] = qp->b[i]/coef->at(0); 

				idx->erase(idx->begin());
				coef->erase(coef->begin()); 
				discard++; 
			}
			else if (idx->size()==1 && qp->eq_idx[i]<0.5)// inequality constraints 
			{
				float eps = 1e-5; 
				if (coef->at(0) > eps)
				{
					assert(qp->lb[idx->at(0)]*coef->at(0) <= qp->b[i]+0.001); 

					if (qp->ub[idx->at(0)]>qp->b[i]/coef->at(0))
						qp->ub[idx->at(0)] = qp->b[i]/coef->at(0); 
				}
				else if (coef->at(0) < -eps)
				{
					assert(qp->ub[idx->at(0)]*coef->at(0) <= qp->b[i]+0.001); 
					if (qp->lb[idx->at(0)]<qp->b[i]/coef->at(0))
						qp->lb[idx->at(0)] = qp->b[i]/coef->at(0); 
				}
				idx->erase(idx->begin());
				coef->erase(coef->begin()); 

				discard++; 
			}	
			
			if (idx->size()>1)
				keep++; 

		}
		if (iter%30==0)
			printf("iteration: %i: discard %i constraints keep:%i delete_coef: %i\n", iter, discard, keep, delete_coef); 
	}
	printf("[%s] reduced number of constraints from %i to %i\n", __func__, qp->b.size(), keep); 
}


vector<double> solve_lp_glpk(QP* qp, bool& success)
{ 

		vector<double> res(qp->num_var, 0);

		/*declarevariables*/
		glp_prob*lp;


		/* convert equality constraints into inequalities */
		// int num_constraints=qp->b.size();
		//int cc = qp->b.size();
		//for(int i=0;i<num_constraints;i++)
		//{
		//	if (qp->eq_idx[i]>0.5)// equality constraints
		//	{
		//		// fill constraint matrix
		//		vector<int>* idx = NULL;
		//		vector<float>* coef = NULL;
		//		int ret = qp->A.get(&idx, &coef, i);
		//		assert(idx!=NULL);
		//		assert(coef!=NULL);
		//		assert(ret>0);
		//		assert(idx->size()==coef->size());

		//		// Copy vectors because insert might cause realloc
		//		vector<int> cidx(idx->begin(), idx->end());
		//		vector<float> ccoef(coef->begin(), coef->end());

		//		for (int j=0; j<cidx.size(); j++)
		//		{
		//			if (cidx.at(j)<0)
		//				printf("i:%i j:%i, idx->at(j):%i, ret:%i\n", i, j, cidx.at(j), ret);
		//			qp->A.set(cc, cidx.at(j), -ccoef.at(j));
		//		}
		//		double b = qp->b[i];
		//		qp->b.push_back(-b);
		//		qp->eq_idx.push_back(0);
		//		qp->eq_idx[i] = 0;
		//		cc++;
		//	}
		//}	
		//assert(cc==qp->b.size());
		//assert(cc==qp->eq_idx.size());

		int num_constraints = 0; 

		int cnt = 1; 
		for(int i=0;i<qp->b.size();i++)
		{
			vector<int>* idx = NULL;
			vector<float>* coef = NULL;
			int ret = qp->A.get(&idx, &coef, i);
			assert(idx!=NULL);
			assert(coef!=NULL);
			assert(ret>0);
			assert(idx->size()==coef->size());

			if (idx->size()>0)
				num_constraints++; 
			for (int j=0; j<idx->size(); j++)
			{
				cnt++;
			}
		}

		printf("num constraints:%lu num coef :%i\n", qp->b.size(), cnt); 
		//double ar[cnt+1];
		//int ia[cnt+1];
		//int ja[cnt+1];
		double* ar = new double[cnt+1]; assert(ar); 
		int* ia = new int[cnt+1]; assert(ia); 
		int* ja = new int[cnt+1]; assert(ja); 

		/*createproblem*/
		lp=glp_create_prob();
		glp_set_prob_name(lp,"mitie_lp");
		glp_set_obj_dir(lp,GLP_MIN);
		/*fillproblem*/
		glp_add_rows(lp,num_constraints);
		//glp_set_row_name(lp,1,"p1");
		cnt = 1;
		int c_cnt = 0; 
		for(int i=0;i<qp->b.size();i++)
		{

			vector<int>* idx = NULL;
			vector<float>* coef = NULL;
			int ret = qp->A.get(&idx, &coef, i);
			assert(idx!=NULL);
			assert(coef!=NULL);
			assert(ret>0);
			assert(idx->size()==coef->size());

			if (idx->size()==0)
				continue; 

			if (qp->eq_idx[i]>0.5)// equality constraints
			{
				//printf("ERRORERRORERROR\n");
				//glp_set_row_bnds(lp,i+1,GLP_LO, qp->b[i] , 0.0);
				//glp_set_row_bnds(lp,i+1,GLP_UP, 0.0 , qp->b[i]);
				//glp_set_row_bnds(lp,i+1,GLP_DB, qp->b[i] , qp->b[i]+1e-5);
				glp_set_row_bnds(lp, c_cnt+1, GLP_FX, qp->b[i] , qp->b[i]);
			}
			else
			{
				glp_set_row_bnds(lp, c_cnt+1, GLP_UP, 0.0 , qp->b[i]);
			}
			// fill constraint matrix

			for (int j=0; j<idx->size(); j++)
			{
				ia[cnt] = c_cnt+1;
				ja[cnt] = idx->at(j)+1;
				ar[cnt] = coef->at(j);
				//if (cnt<5)
				//	printf("ia[%i]:%i ja[%i]%i ar[%i]%f\n", cnt, ia[cnt], cnt, ja[cnt], cnt, ar[cnt]); 

				cnt++;
			}
			c_cnt++; 
		}
		assert(c_cnt==num_constraints); 

		bool any_binary = false; 
		glp_add_cols(lp, qp->num_var);
		for(int i=0;i<qp->num_var;i++)
		{
			if (qp->binary_idx[i]==1 && qp->lb[i]!=qp->ub[i])
			{
				any_binary = true; 
				glp_set_col_kind(lp,i+1,GLP_BV);
			}
			assert(qp->lb[i]<=qp->ub[i]); 
			// set box constraints for variables
			if (qp->lb[i]==qp->ub[i])
			{
				glp_set_col_bnds(lp,i+1,GLP_FX,qp->lb[i],qp->ub[i]);
				//glp_set_col_kind(lp,i+1,GLP_BV);
				//glp_set_col_bnds(lp,i+1,GLP_DB, qp->lb[i]-1e-5, qp->ub[i]+1e-5);
				//glp_set_col_bnds(lp,i+1,GLP_LO, qp->lb[i]-1e-5, 0.0);
				//glp_set_col_bnds(lp,i+1,GLP_UP, 0.0, qp->ub[i]+1e-5);
			}
			else if (qp->lb[i]>-1e10 && qp->ub[i]<1e10)
			{
				glp_set_col_bnds(lp,i+1,GLP_DB,qp->lb[i],qp->ub[i]);
			}
			else if (qp->lb[i]>-1e10)
			{
				glp_set_col_bnds(lp,i+1,GLP_LO,qp->lb[i],0.0);
			}
			else if (qp->ub[i]<1e10)
			{
				glp_set_col_bnds(lp,i+1,GLP_UP,0.0,qp->ub[i]);
			}
			else 
			{
				glp_set_col_bnds(lp,i+1,GLP_FR,0.0,0.0);
			}

			if (qp->F[i]!=0)
			{
				//printf("set objective coef[%i+1] %.5f\n", i, qp->F[i]);
				glp_set_obj_coef(lp,i+1, qp->F[i]);
			}
		}

		printf("num_constraints: %i, cnt:%i\n", num_constraints, cnt-1);
		glp_load_matrix(lp,cnt-1,ia,ja,ar);

		/*solveproblem*/
		// only needed if presolve is off
		glp_simplex(lp,NULL);
		double z1 = glp_get_obj_val(lp);
		printf("glpk simplex obj: %.3f\n", z1);

		if (any_binary)
		{
			glp_iocp parm;
			glp_init_iocp(&parm);
			parm.presolve = GLP_ON;
			parm.cov_cuts = GLP_ON;
			parm.gmi_cuts = GLP_ON; 
			parm.mir_cuts = GLP_ON; 
			parm.clq_cuts = GLP_ON; 
			int err = glp_intopt(lp, &parm);
 		
			/*recover results*/
			double z = glp_mip_obj_val(lp);
			int status = glp_mip_status(lp); 
			success = status==GLP_OPT; 
			printf("glpk mip obj: %.3f status: %i %i %i %i %i \n", z, status, GLP_UNDEF, GLP_OPT, GLP_FEAS, GLP_NOFEAS);
 		}

		printf("glpk_version: %s\n", glp_version());
#ifdef DEBUG_GLPK
		glp_write_mip(lp, "/home/behrj/tmp/glpk_mip_result.txt");
		int flag = 0;//this is ignored according to the glpk manual
		//int ret = glp_write_prob(lp, flag, "/home/behrj/tmp/glpk_mip_problem_cplex.glpk");
		int ret = glp_write_lp(lp, NULL, "/home/behrj/tmp/glpk_mip_problem_cplex.glpk");
		//assert(ret==0);
#endif
                      
		for(int nt=0;nt<qp->num_var;nt++) 
		{
			if (any_binary)
				res[nt]=glp_mip_col_val(lp, nt+1);
			else
				res[nt]=glp_get_col_prim(lp, nt+1);
		}

		//glp_write_prob(lp, 0, "temp_prob"); 	

		/*housekeeping*/
		glp_delete_prob(lp);
		glp_free_env();
		delete ar; 
		delete ia; 
		delete ja; 


//#ifdef DEBUG_GLPK
//		// check constraints
//		for(int i=0;i<num_constraints;i++)
//		{
//			
//			vector<int>* idx = NULL;
//			vector<float>* coef = NULL;
//			int ret = qp->A.get(&idx, &coef, i);
//			assert(idx!=NULL);
//			assert(coef!=NULL);
//			assert(ret>0);
//			assert(idx->size()==coef->size());
//
//			double left = 0.0;
//			for (int j=0; j<idx->size(); j++)
//			{
//				//printf("%i->%f ", idx->at(j)+1, coef->at(j));
//				left += coef->at(j)*res[idx->at(j)]; 
//			}
//			if (qp->eq_idx[i]>0.5 && (left-qp->b[i]>1e-2 || qp->b[i]-left>1e-2))
//			{
//				printf("violated constraint %i: left:%.3f, b:%.3f equality:%i\n", i, left, qp->b[i], qp->eq_idx[i]);
//			}
//			else if (left-qp->b[i]>1e-2)
//			{
//				printf("violated constraint %i: left:%.3f, b:%.3f equality:%i\n", i, left, qp->b[i], qp->eq_idx[i]);
//				for (int j=0; j<idx->size(); j++)
//					printf("%i->%.3f res:%.3f ", idx->at(j), coef->at(j), res[idx->at(j)]);
//			}
//		}
//
//		// compute objective
//		double obj = 0.0;
//		for(int i=0;i<qp->num_var;i++)
//		{
//		 	obj+=qp->F[i]*res[i];
//		}
//		printf("my obj is: %.3f\n", obj); 
//
//		//check box constraints
//		for(int i=0;i<qp->num_var;i++)
//		{
//			if ( res[i]>qp->lb[i]-1e-3 && res[i]<qp->ub[i]+1e-3)
//			{}
//			else 
//				printf("box constraint violated: %i (%.3f<=%.3f<=%.3f) binary:%i\n", i, qp->lb[i], res[i], qp->ub[i], qp->binary_idx[i]);
//			if (qp->binary_idx[i]>0.5)
//			{
//				if ((res[i]-1>1e-3 || 1-res[i]>1e-3) && (res[i]>1e-3 || -res[i]>1e-3))
//					printf("binary constraint violated: %i\n", i);
//			}		
//		}
//#endif

		return res;
}
