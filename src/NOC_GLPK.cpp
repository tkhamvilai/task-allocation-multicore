//
// Created by tkhamvilai on 4/12/19.
//

#include "NOC_GLPK.hpp"

#ifdef GLPK_AS_SOLVER
NOC_GLPK::NOC_GLPK(const char* LP_file, const char* Sol_file)
{
    this->LP_file = LP_file;
    this->Sol_file = Sol_file;
}

void NOC_GLPK::write_LP(NOC *NoC)
{
    this->model = glp_create_prob();
    glp_set_prob_name(this->model, "NoC");

    CreateModel(NoC);
    glp_write_lp(this->model, NULL, this->LP_file);
}

void NOC_GLPK::CreateModel(NOC *NoC)
{
    glp_set_obj_dir(this->model, GLP_MAX);

    /*
     * Create Decision Variables
     */

    for (int i = 0; i < NoC->N_CRs; i++)
    {
        for (int j = 0; j < NoC->N_nodes; j++)
        {
            NoC->var_size += 1;
            std::string name = "X_CR_" + std::to_string(i) + "_node_" + std::to_string(j);
            glp_add_cols(model, 1);
            glp_set_col_name(this->model, NoC->var_size, name.c_str());
            glp_set_col_kind(this->model, NoC->var_size, GLP_BV);
            glp_set_obj_coef(this->model, NoC->var_size, 0.0);
        }
    }

    for (int i = 0; i < NoC->N_paths; i++)
    {
        for (int j = 0; j < NoC->N_links; j++)
        {
            NoC->var_size += 1;
            std::string name = "X_path_" + std::to_string(i) + "_link_" + std::to_string(j);
            glp_add_cols(model, 1);
            glp_set_col_name(model, NoC->var_size, name.c_str());
            glp_set_col_kind(model, NoC->var_size, GLP_BV);
            glp_set_obj_coef(model, NoC->var_size, 0.0);
        }
    }

    int *coeff = new int[NoC->N_apps];
    glp_add_cols(model, NoC->N_apps);
    NoC->var_size += NoC->N_apps;
    for (int i = NoC->N_apps; i > 0; i--)
    {
        std::string name = "R_apps_" + std::to_string(i-1);
        glp_set_col_name(model, NoC->var_size - (NoC->N_apps - i), name.c_str());
        glp_set_col_kind(model, NoC->var_size - (NoC->N_apps - i), GLP_BV);
        coeff[i-1] = NoC->N_nodes*(NoC->N_paths*NoC->N_CRs*NoC->allocator_app_num + 1) + 1;
        for (int j = i; j < NoC->N_apps; j++)
        {
            coeff[i-1] += coeff[j];
        }
        glp_set_obj_coef(model, NoC->var_size - (NoC->N_apps - i), coeff[i-1]);
    }

    for (int i = 0; i < NoC->N_nodes; i++)
    {
        NoC->var_size += 1;
        std::string name = "M_apps_" + std::to_string(i);
        glp_add_cols(model, 1);
        glp_set_col_name(model, NoC->var_size, name.c_str());
        glp_set_col_kind(model, NoC->var_size, GLP_BV);
        glp_set_obj_coef(model, NoC->var_size, -(NoC->N_paths*NoC->N_CRs*NoC->allocator_app_num + 1));
    }

    for(int k = 0; k < NoC->allocator_app_num; k++)
    {
        for (int i = 0; i < NoC->N_paths; i++)
        {
            for (int j = 0; j < NoC->N_CRs; j++)
            {
                NoC->var_size += 1;
                std::string name = "X_comm_path_" + std::to_string(i) + "_CR_" + std::to_string(j) + "_alloc_" + std::to_string(k);
                glp_add_cols(model, 1);
                glp_set_col_name(model, NoC->var_size, name.c_str());
                glp_set_col_kind(model, NoC->var_size, GLP_IV);
                glp_set_obj_coef(model, NoC->var_size, 0.0);
                glp_set_col_bnds(model, NoC->var_size, GLP_DB, -1.0, 1.0);
            }
        }
    }

    for(int k = 0; k < NoC->allocator_app_num; k++)
    {
        for (int i = 0; i < NoC->N_paths; i++)
        {
            for (int j = 0; j < NoC->N_CRs; j++)
            {
                NoC->var_size += 1;
                std::string name = "Dummy_X_comm_path_" + std::to_string(i) + "_CR_" + std::to_string(j) + "_alloc_" + std::to_string(k);
                glp_add_cols(model, 1);
                glp_set_col_name(model, NoC->var_size, name.c_str());
                glp_set_col_kind(model, NoC->var_size, GLP_IV);
                glp_set_obj_coef(model, NoC->var_size, -1.0);
                glp_set_col_bnds(model, NoC->var_size, GLP_LO, 0.0, 0.0);
            }
        }
    }

    /*
     * Create Constraints
     */
    int max_size = 99999+1;
    int ia[max_size], ja[max_size];
    int ind_count = 0;
    double ar[max_size];

    // Resource allocation and partitioning
    for (int i = 1; i <= NoC->N_CRs; i++)
    {
        NoC->con_size += 1;
        std::string name = "CR_" + std::to_string(i-1) + "_sum_in_nodes";
        glp_add_rows(this->model, 1);
        glp_set_row_name(this->model, NoC->con_size, name.c_str());

        if(NoC->Fault_CRs[i-1] == 0 && NoC->Fault_External_CRs[i-1] == 0) //&& NoC->D(i-1, i-1) > 0)
        {
            glp_set_row_bnds(this->model, NoC->con_size, GLP_UP, 0.0, 1.0);
        }
        else
        {
            glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);
        }

        for (int j = 1; j <= NoC->N_nodes; j++)
        {
            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = j + NoC->N_nodes*(i-1);
            ar[ind_count] = 1;
        }
    }

    for (int i = 1; i <= NoC->N_nodes; i++)
    {
        NoC->con_size += 1;
        std::string name = "node_" + std::to_string(i-1) + "_sum_in_CRs";
        glp_add_rows(this->model, 1);
        glp_set_row_name(this->model, NoC->con_size, name.c_str());
        glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

        for (int j = i; j <= NoC->N_CRs*NoC->N_nodes; j+= NoC->N_nodes)
        {
            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = j;
            ar[ind_count] = 1;
        }

        ind_count += 1;
        ia[ind_count] = NoC->con_size;
        ja[ind_count] = NoC->get_app_from_node(i) + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links;
        ar[ind_count] = -1;
    }

    for (int i = 1; i <= NoC->N_paths; i++)
    {
        NoC->con_size += 1;
        std::string name = "path_" + std::to_string(i-1) + "_sum_in_links";
        glp_add_rows(this->model, 1);
        glp_set_row_name(this->model, NoC->con_size, name.c_str());
        glp_set_row_bnds(this->model, NoC->con_size, GLP_UP, 0.0, 1.0);

        for (int j = 1; j <= NoC->N_links; j++)
        {
            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = j + NoC->N_links*(i-1) + NoC->N_CRs*NoC->N_nodes;
            ar[ind_count] = 1;
        }
    }

    for (int i = 1; i <= NoC->N_links; i++)
    {
        NoC->con_size += 1;
        std::string name = "link_" + std::to_string(i-1) + "_sum_in_paths";
        glp_add_rows(this->model, 1);
        glp_set_row_name(this->model, NoC->con_size, name.c_str());
        glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

        for (int j = i; j <= NoC->N_paths*NoC->N_links; j+= NoC->N_links)
        {
            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = j + NoC->N_CRs*NoC->N_nodes;
            ar[ind_count] = 1;
        }

        ind_count += 1;
        ia[ind_count] = NoC->con_size;
        ja[ind_count] = NoC->get_app_from_link(i-1) + 1 + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links;
        ar[ind_count] = -1;
    }

    // Conformity to the architecture
    for (int i = 1; i <= NoC->N_CRs; i++)
    {
        for (int j = 1; j <= NoC->N_links; j++)
        {
            NoC->con_size += 1;
            std::string name = "Conformity_" + std::to_string(i-1) + "_" + std::to_string(j-1);
            glp_add_rows(this->model, 1);
            glp_set_row_name(this->model, NoC->con_size, name.c_str());
            glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

            for (int k = 1; k <= NoC->N_nodes; k++)
            {
                ind_count += 1;
                ia[ind_count] = NoC->con_size;
                ja[ind_count] = k + NoC->N_nodes*(i-1);
                ar[ind_count] = abs(NoC->H(k-1,j-1));
            }

            for (int k = 1; k <= NoC->N_paths; k++)
            {
                ind_count += 1;
                ia[ind_count] = NoC->con_size;
                ja[ind_count] = j + NoC->N_links*(k-1) + NoC->N_CRs*NoC->N_nodes;
                ar[ind_count] = -abs(NoC->G(i-1,k-1));
            }
        }
    }

    // Reallocating several applications
    for (int i = 1; i <= NoC->N_CRs; i++)
    {
        for (int j = 1; j <= NoC->N_nodes; j++)
        {
            if(NoC->X_CRs_nodes_old(i-1,j-1) == 1)
            {
                NoC->con_size += 1;
                std::string name = "Reallocate _Apps_" + std::to_string(i-1) + "_" + std::to_string(j-1);
                glp_add_rows(this->model, 1);
                glp_set_row_name(this->model, NoC->con_size, name.c_str());

                if (NoC->N_Faults_CR == 0)
                {
                    glp_set_row_bnds(this->model, NoC->con_size, GLP_LO, NoC->X_CRs_nodes_old(i-1, j-1) - 1.0, 0.0);
                }
                else
                {
                    glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, NoC->X_CRs_nodes_old(i-1, j-1) - 1.0, NoC->X_CRs_nodes_old(i-1, j-1) - 1.0);
                }

                ind_count += 1;
                ia[ind_count] = NoC->con_size;
                ja[ind_count] = NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links + NoC->get_app_from_node(j);
                ar[ind_count] = -1;

                ind_count += 1;
                ia[ind_count] = NoC->con_size;
                ja[ind_count] = j + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links + NoC->N_apps;
                ar[ind_count] = 1;

                ind_count += 1;
                ia[ind_count] = NoC->con_size;
                ja[ind_count] = j + NoC->N_nodes*(i-1);
                ar[ind_count] = 1;
            }
        }
    }

    // Priorities
    for (int i = 1; i <= 2; i++)
    {
        NoC->con_size += 1;
        std::string name = "Priority_" + std::to_string(i-1);
        glp_add_rows(this->model, 1);
        glp_set_row_name(this->model, NoC->con_size, name.c_str());

        if(i == 1)
        {
            glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = i + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links;
            ar[ind_count] = 1;

            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = i + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links + 1;
            ar[ind_count] = -1;

//            glp_set_row_bnds(this->model, NoC->con_size, GLP_LO, 1.0, 0.0);
//
//            ind_count += 1;
//            ia[ind_count] = NoC->con_size;
//            ja[ind_count] = i + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links;
//            ar[ind_count] = 1;
        }
        else
        {
            glp_set_row_bnds(this->model, NoC->con_size, GLP_LO, 0.0, 0.0);

            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = i + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links;
            ar[ind_count] = 1;

            ind_count += 1;
            ia[ind_count] = NoC->con_size;
            ja[ind_count] = i + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links + 1;
            ar[ind_count] = -1;

//            ind_count += 1;
//            ia[ind_count] = NoC->con_size;
//            ja[ind_count] = i + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links;
//            ar[ind_count] = -1;
//
//            ind_count += 1;
//            ia[ind_count] = NoC->con_size;
//            ja[ind_count] = (i-1) + NoC->N_CRs*NoC->N_nodes + NoC->N_paths*NoC->N_links;
//            ar[ind_count] = 1;
        }
    }

    // Spatial Orientation
    int sum_N_nodes_apps = 0;
    int spatial_nodes_ind[3]; // indices of the interested spatial nodes
    for (int k = 1; k <= NoC->N_apps; k++)
    {
        spatial_nodes_ind[0] = sum_N_nodes_apps; // top left
        spatial_nodes_ind[1] = sum_N_nodes_apps + 1; // node to the right of the top left one
        spatial_nodes_ind[2] = sum_N_nodes_apps + NoC->N_Col_apps[k-1] ; // node below the top left one

        sum_N_nodes_apps += NoC->N_nodes_apps[k-1];

        for (int i = 1; i < NoC->N_CRs; i++)
        {
            for (int j = 1; j < NoC->N_nodes_apps[k-1]; j++)
            {
                try
                {
                    if (i % NoC->N_Col_CRs != 0 && NoC->N_Col_apps[k - 1] > 1 && j % NoC->N_Col_apps[k-1]) // not the rightmost col && app takes more than one col
                    {
                        NoC->con_size += 1;
                        std::string name =
                                "Spatial_Right_CR_" + std::to_string(i - 1) + "_app_" + std::to_string(k - 1);
                        glp_add_rows(this->model, 1);
                        glp_set_row_name(this->model, NoC->con_size, name.c_str());
                        glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

                        ind_count += 1;
                        ia[ind_count] = NoC->con_size;
                        ja[ind_count] = spatial_nodes_ind[0] + NoC->N_nodes * (i - 1) + j;
                        ar[ind_count] = 1;

                        ind_count += 1;
                        ia[ind_count] = NoC->con_size;
                        ja[ind_count] = spatial_nodes_ind[1] + NoC->N_nodes * (i) + j;
                        ar[ind_count] = -1;
                    }
                }
                catch (...)
                {}

                try
                {
                    if ((i - 1) < (NoC->N_CRs - NoC->N_Col_CRs) && NoC->N_Row_apps[k - 1] > 1 && j % NoC->N_Row_apps[k-1]) // not the bottom row && app takes more than one row
                    {
                        NoC->con_size += 1;
                        std::string name =
                                "Spatial_Below_CR_" + std::to_string(i - 1) + "_app_" + std::to_string(k - 1);
                        glp_add_rows(this->model, 1);
                        glp_set_row_name(this->model, NoC->con_size, name.c_str());
                        glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

                        ind_count += 1;
                        ia[ind_count] = NoC->con_size;
                        ja[ind_count] = spatial_nodes_ind[0] + NoC->N_nodes * (i - 1) + j;
                        ar[ind_count] = 1;

                        ind_count += 1;
                        ia[ind_count] = NoC->con_size;
                        ja[ind_count] = spatial_nodes_ind[2] + NoC->N_nodes * (i - 1 + NoC->N_Col_CRs) + j;
                        ar[ind_count] = -1;
                    }
                }
                catch (...)
                {}
            }
        }
    }

/*    int sum_N_nodes_apps = 0;
    int spatial_nodes_ind[3]; // indices of the interested spatial nodes
    for (int k = 1; k <= NoC->N_apps; k++)
    {
        spatial_nodes_ind[0] = sum_N_nodes_apps; // top left
        spatial_nodes_ind[1] = sum_N_nodes_apps + 1; // node to the right of the top left one
        spatial_nodes_ind[2] = sum_N_nodes_apps + NoC->N_Col_apps[k-1] ; // node below the top left one

        sum_N_nodes_apps += NoC->N_nodes_apps[k-1];

        for (int i = 1; i < NoC->N_CRs; i++)
        {
            try
            {
                if(i % NoC->N_Col_CRs != 0 && NoC->N_Col_apps[k-1] > 1) // not the rightmost col && app takes more than one col
                {
                    NoC->con_size += 1;
                    std::string name = "Spatial_Right_CR_" + std::to_string(i-1) + "_app_" + std::to_string(k-1);
                    glp_add_rows(this->model, 1);
                    glp_set_row_name(this->model, NoC->con_size, name.c_str());
                    glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

                    ind_count += 1;
                    ia[ind_count] = NoC->con_size;
                    ja[ind_count] = spatial_nodes_ind[0] + NoC->N_nodes*(i-1) + 1;
                    ar[ind_count] = 1;

                    ind_count += 1;
                    ia[ind_count] = NoC->con_size;
                    ja[ind_count] = spatial_nodes_ind[1] + NoC->N_nodes*(i) + 1;
                    ar[ind_count] = -1;
                }
            }
            catch(...) {}

            try
            {
                if((i-1) < (NoC->N_CRs - NoC->N_Col_CRs) && NoC->N_Row_apps[k-1] > 1) // not the bottom row && app takes more than one row
                {
                    NoC->con_size += 1;
                    std::string name = "Spatial_Below_CR_" + std::to_string(i-1) + "_app_" + std::to_string(k-1);
                    glp_add_rows(this->model, 1);
                    glp_set_row_name(this->model, NoC->con_size, name.c_str());
                    glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

                    ind_count += 1;
                    ia[ind_count] = NoC->con_size;
                    ja[ind_count] = spatial_nodes_ind[0] + NoC->N_nodes*(i-1) + 1;
                    ar[ind_count] = 1;

                    ind_count += 1;
                    ia[ind_count] = NoC->con_size;
                    ja[ind_count] = spatial_nodes_ind[2] + NoC->N_nodes*(i - 1 + NoC->N_Col_CRs) + 1;
                    ar[ind_count] = -1;
                }
            }
            catch(...) {}
        }
    }*/

    // Communication Constraint
    int allocator_node_ind = 0;
    for (int i = 0; i < NoC->allocator_app_ind; i++)
    {
        allocator_node_ind += NoC->N_nodes_apps[i];
    }
    for (int k = 0; k < NoC->allocator_app_num; k++)
    {
        for (int i = 1; i <= NoC->N_CRs; i++) // sum of all paths passing through i^th CR
        {
            if(NoC->Fault_CRs[i-1] == 0)//(NoC->D(i-1, i-1) > 0) // no faults
            {
                for (int j = 1; j <= NoC->N_CRs; j++) // when j^th CR being a sink CR
                {
                    if(NoC->Fault_CRs[j-1] == 0 && i != j)//(NoC->D(j-1, j-1) > 0) // sink has to be alive; otherwise, don't draw a path
                    {
                        NoC->con_size += 1;
                        std::string name = "Comm_from_" + std::to_string(i - 1) + "_to_" + std::to_string(j - 1) + "_alloc_" + std::to_string(k);
                        glp_add_rows(this->model, 1);
                        glp_set_row_name(this->model, NoC->con_size, name.c_str());

//                        if (i == j)
//                        {
//                            glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 1.0, 1.0);
//                        }
//                        else
//                        {
                            glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);
//                        }

                        for (int l = 1; l <= NoC->N_paths; l++)
                        {
                            ind_count += 1;
                            ia[ind_count] = NoC->con_size;
                            ja[ind_count] =
                                    j + NoC->N_CRs * (l - 1) + NoC->N_paths * NoC->N_CRs * k +
                                    NoC->N_CRs * NoC->N_nodes +
                                    NoC->N_paths * NoC->N_links + NoC->N_apps + NoC->N_nodes;
                            ar[ind_count] = NoC->G(i - 1, l - 1);
                        }

                        ind_count += 1;
                        ia[ind_count] = NoC->con_size;
                        ja[ind_count] = NoC->N_nodes * (i - 1) + (allocator_node_ind + k);
                        ar[ind_count] = 1;
                    }
                }
            }
            else // dead CRs or isolated
            {
                for (int j = 1; j <= NoC->N_paths; j++) // not drawing a path to a faulty CR
                {
                    NoC->con_size += 1;
                    std::string name = "No_Comm_passing_through_" + std::to_string(i - 1) + "_from_path_" + std::to_string(j - 1) + "_alloc_" + std::to_string(k);
                    glp_add_rows(this->model, 1);
                    glp_set_row_name(this->model, NoC->con_size, name.c_str());
                    glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

                    ind_count += 1;
                    ia[ind_count] = NoC->con_size;
                    ja[ind_count] = i + NoC->N_CRs * (j - 1) + NoC->N_paths * NoC->N_CRs * k + NoC->N_CRs * NoC->N_nodes + NoC->N_paths * NoC->N_links + NoC->N_apps + NoC->N_nodes;
                    ar[ind_count] = 1;
                }
            }

            for (int j = 1; j <= NoC->N_paths; j++)
            {
                if(NoC->Fault_Paths[j-1] == 1)
                {
                    NoC->con_size += 1;
                    std::string name = "No_Comm_from_path_" + std::to_string(j - 1) + "_to_CR_" + std::to_string(i - 1) + "_alloc_" + std::to_string(k);
                    glp_add_rows(this->model, 1);
                    glp_set_row_name(this->model, NoC->con_size, name.c_str());
                    glp_set_row_bnds(this->model, NoC->con_size, GLP_FX, 0.0, 0.0);

                    ind_count += 1;
                    ia[ind_count] = NoC->con_size;
                    ja[ind_count] = i + NoC->N_CRs * (j - 1) + NoC->N_paths * NoC->N_CRs * k + NoC->N_CRs * NoC->N_nodes + NoC->N_paths * NoC->N_links + NoC->N_apps + NoC->N_nodes;
                    ar[ind_count] = 1;
                }
            }
        }
    }

    // Dummy Constraints for Shortest Path
    for (int i = 1; i <= NoC->N_paths*NoC->N_CRs*NoC->allocator_app_num; i++)
    {
        NoC->con_size += 1;
        std::string name = "Positive_Dummy" + std::to_string(i - 1);
        glp_add_rows(this->model, 1);
        glp_set_row_name(this->model, NoC->con_size, name.c_str());
        glp_set_row_bnds(this->model, NoC->con_size, GLP_UP, 0.0, 0.0);

        ind_count += 1;
        ia[ind_count] = NoC->con_size;
        ja[ind_count] = i + NoC->N_CRs * NoC->N_nodes + NoC->N_paths * NoC->N_links + NoC->N_apps + NoC->N_nodes;
        ar[ind_count] = 1;

        ind_count += 1;
        ia[ind_count] = NoC->con_size;
        ja[ind_count] = i + NoC->N_paths*NoC->N_CRs*NoC->allocator_app_num + NoC->N_CRs * NoC->N_nodes + NoC->N_paths * NoC->N_links + NoC->N_apps + NoC->N_nodes;
        ar[ind_count] = -1;
    }

    for (int i = 1; i <= NoC->N_paths*NoC->N_CRs*NoC->allocator_app_num; i++)
    {
        NoC->con_size += 1;
        std::string name = "Negative_Dummy" + std::to_string(i - 1);
        glp_add_rows(this->model, 1);
        glp_set_row_name(this->model, NoC->con_size, name.c_str());
        glp_set_row_bnds(this->model, NoC->con_size, GLP_UP, 0.0, 0.0);

        ind_count += 1;
        ia[ind_count] = NoC->con_size;
        ja[ind_count] = i + NoC->N_CRs * NoC->N_nodes + NoC->N_paths * NoC->N_links + NoC->N_apps + NoC->N_nodes;
        ar[ind_count] = -1;

        ind_count += 1;
        ia[ind_count] = NoC->con_size;
        ja[ind_count] = i + NoC->N_paths*NoC->N_CRs*NoC->allocator_app_num + NoC->N_CRs * NoC->N_nodes + NoC->N_paths * NoC->N_links + NoC->N_apps + NoC->N_nodes;
        ar[ind_count] = -1;
    }

//    std::cout << ind_count << std::endl;
    glp_load_matrix(this->model, ind_count, ia, ja, ar);
}

void NOC_GLPK::read_Sol(NOC *NoC)
{
//    glp_prob *mip = glp_create_prob();
//    glp_read_mip(mip, Sol_file);

    NoC->obj_val = (int)glp_mip_obj_val(this->model);

    int k = 1;
    for (int i = 0; i < NoC->N_CRs; i++)
    {
        for (int j = 0; j < NoC->N_nodes; j++)
        {
            NoC->X_CRs_nodes(i,j) = (int)glp_mip_col_val(this->model, k);
            k++;
        }
    }

    for (int i = 0; i < NoC->N_paths; i++)
    {
        for (int j = 0; j < NoC->N_links; j++)
        {
            NoC->X_paths_links(i,j) = (int)glp_mip_col_val(this->model, k);
            k++;
        }
    }

    NoC->X_CRs_nodes_old = NoC->X_CRs_nodes;

    for (int i = 0; i < NoC->N_apps; i++)
    {
        NoC->R_apps(i) = (int)glp_mip_col_val(this->model, k);
        k++;
    }

    for (int i = 0; i < NoC->N_nodes; i++)
    {
        NoC->M_apps(i) = (int)glp_mip_col_val(this->model, k);
        k++;
    }

    for (int l = 0; l < NoC->allocator_app_num; l++)
    {
        Eigen::MatrixXi X_comm_paths_temp(NoC->N_paths, NoC->N_CRs);
        for (int i = 0; i < NoC->N_paths; i++)
        {
            for (int j = 0; j < NoC->N_CRs; j++)
            {
                X_comm_paths_temp(i,j) = (int)glp_mip_col_val(this->model, k);
                k++;
            }
        }
        NoC->X_comm_paths[l] = X_comm_paths_temp;
    }

    DeleteModel(NoC);
}

void NOC_GLPK::DeleteModel(NOC *NoC)
{
    glp_delete_prob(this->model);
    NoC->var_size = 0;
    NoC->con_size = 0;
}
#endif