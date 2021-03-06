#include "MY_MACROS.hpp"
#include "MY_INCLUDE.hpp"
#include "APP_INCLUDE.hpp"

#ifndef __x86_64__
void sighandler(int signal)
{
    APP_LED_OFF();
    PATH_LED_OFF();
    APP_PWM_OFF();
    exit(1);
}
#endif

int main (int argc, char* argv[])
{
    /** User-Initialized Parameters **/

    int N_Row_CRs = 4, N_Col_CRs = 4;
    int N_apps = 7;

    int N_Row_apps[N_apps], N_Col_apps[N_apps], app_color[N_apps];
    N_Row_apps[0] = 2;    N_Col_apps[0] = 1;
    N_Row_apps[1] = 2;    N_Col_apps[1] = 1;
    N_Row_apps[2] = 2;    N_Col_apps[2] = 1;
    N_Row_apps[3] = 1;    N_Col_apps[3] = 1;
    N_Row_apps[4] = 1;    N_Col_apps[4] = 1;
    N_Row_apps[5] = 1;    N_Col_apps[5] = 1;
    N_Row_apps[6] = 1;    N_Col_apps[6] = 2;

    void (*app_ptr[N_apps + 1])(NOC*, NOC_FAULT*, NOC_GLPK*, GLPK_SOLVER*, ENGINE*, int);
    app_ptr[0] = &APP_LED;          app_color[0] = LED_WHITE;   // IDLE
    app_ptr[1] = &APP_PID;          app_color[1] = LED_RED;     // 1st priority app
    app_ptr[2] = &APP_PID;          app_color[2] = LED_GREEN;   // and so on...
    app_ptr[3] = &APP_PID;          app_color[3] = LED_BLUE;
    app_ptr[4] = &APP_REALLOCATOR;  app_color[4] = LED_RED;
    app_ptr[5] = &APP_REALLOCATOR;  app_color[5] = LED_GREEN;
    app_ptr[6] = &APP_REALLOCATOR;  app_color[6] = LED_BLUE;
    app_ptr[7] = &APP_LED;          app_color[7] = LED_MAGENTA;

    // index of app where the first allocator starts, and the total number allocators
    int allocator_app_ind = 4, allocator_app_num = 3;

    const char* LP_file = "NoC.lp"; // problem file name
    const char* Sol_file = "sol.txt"; // solution file name

    /** End of User Initialization **/

    /*
     * Initialization
     */

    NOC_MPI NoC_MPI = NOC_MPI(); // NoC MPI Object
//    std::cout << NoC_MPI.world_rank << std::endl;

    NOC NoC = NOC(N_Row_CRs, N_Col_CRs, N_apps, N_Row_apps, N_Col_apps, app_color, allocator_app_ind, allocator_app_num); // NoC Object
    NoC.CreateTopology("square");

    NOC_FAULT NoC_Fault = NOC_FAULT(&NoC, NoC_MPI.world_rank); // NoC Fault Detection Object

    NOC_GLPK NoC_GLPK = NOC_GLPK(LP_file, Sol_file); // NoC to GLPK Object
    GLPK_SOLVER prob_GLPK = GLPK_SOLVER(LP_file, Sol_file); // Solver Object

    ENGINE Engine = ENGINE(NoC.N_CRs); // Engine controller being the 1st priority app

#ifdef USE_MPI
#ifndef __x86_64__
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);
    wiringPiSetup();
#endif

    if (NoC_MPI.world_rank == 1) // first allocation
    {
        NoC_GLPK.write_LP(&NoC);
        prob_GLPK.solve(&NoC_GLPK);
        NoC_GLPK.read_Sol(&NoC);
        NoC.Update_State();
        NoC.Disp();
    }
    NoC_MPI.run(&NoC, &Engine);

    /** End of Initialization **/

    /*
     * Main Loop
     */

    int step = 0;
    while (true)
    {
#if defined (__x86_64__)
#if ( PRINT )
        std::cout << "Step: " << step << ", ";
#endif
#endif
        if (NoC_MPI.world_rank == 0)
        {
            Engine.run(NoC.N_CRs);
        }
        else // computer resource nodes
        {
            NoC.Voter(NoC_MPI.world_rank, step); // vote on reallocator signals
#if defined (__x86_64__)
#if ( PRINT ) // print the simulation
            std::cout << "My Rank: " << NoC_MPI.world_rank;
//            std::cout << ", My Fault: " << NoC.fault_internal_status;
            std::cout << ", My Node: " << NoC.node_to_run;
//            std::cout << ", My Sensor: " << Engine.sensor_data;
            std::cout << ", My App: " << NoC.app_to_run << ", ";
#endif
#endif
            int fault_internal_status = NoC_Fault.Fault_Detection(&NoC, NoC_MPI.world_rank);

            if(NoC.app_to_run == -1 || fault_internal_status == 1) // dead
            {
                APP_LED_OFF();
                PATH_LED_OFF();
                NoC.Clear_State();
                NoC.node_to_run = -1; // The dead won't remember anything
            }
            else
            {
                app_ptr[NoC.app_to_run](&NoC, &NoC_Fault, &NoC_GLPK, &prob_GLPK, &Engine, NoC.app_color[NoC.app_to_run]);
                PATH_LED(NoC.path_to_run, NoC.G, NoC_MPI.world_rank);

                if(!(NoC.app_to_run >= NoC.allocator_app_ind && NoC.app_to_run < NoC.allocator_app_ind + NoC.allocator_app_num))
                {
                    NoC.Clear_State(); // not the allocator
                }
            }
        }

        NoC_MPI.run(&NoC, &Engine); // Communication Scheme
        step++;

#ifdef __x86_64__ // only put the delay on the simulation
#ifndef USE_X_PLANE_SIMULATOR
        usleep(1000000);
#endif
#endif
    }
#else
#ifdef USE_X_PLANE_SIMULATOR
    Engine.udp.init(X_PLANE_IP_ADDRESS, "192.168.0.130", X_PLANE_PORT, X_PLANE_PORT);
#endif
    NoC.app_to_run = NoC.allocator_app_ind; // always runs the allocator for debugging optimization problem
    while (true)
    {
#ifdef USE_X_PLANE_SIMULATOR // for testing the controller
        if(Engine.udp.initYet) Engine.udp.receivePacket(Engine.data, X_PLANE_PACKET_BYTE);
        Engine.roll_deg = Decode_Roll_X_plane (Engine.data);
        Engine.roll_dot = Decode_Roll_Dot_X_plane (Engine.data);
        std::cout << "roll_deg = " << Engine.roll_deg << std::endl;
        std::cout << "roll_dot = " << Engine.roll_dot << std::endl;
        float setpoint = 10.0, del_a;
        del_a = (setpoint - Engine.roll_deg)*40.2 - 22.5*Engine.roll_dot;
        del_a = 0.5*0.011111*del_a;
        Engine.delta_a_out = del_a;
        std::cout << Engine.delta_a_out << std::endl;
        Encode_Delta_to_X_plane(Engine.delta_a_out, Engine.buf);
        Engine.udp.sendPacket(Engine.buf, 41);
#endif
        if (NoC_Fault.Fault_Gathering(&NoC)) // get fault data from others
        {
            NoC_GLPK.write_LP(&NoC);
            if(prob_GLPK.solve(&NoC_GLPK))
            {
                NoC_GLPK.read_Sol(&NoC);
                NoC.solver_status = 1;
            }
            else
            {
                std::cout << "Infeasible Solution" << std::endl;
                NoC.solver_status = 0;
            }
            NoC.Update_State();
        }
        NoC.Disp();
    }
#endif

    while (true){usleep(1000000);} // does nothing, but smiling at you :)

    return 0;

}  // END main