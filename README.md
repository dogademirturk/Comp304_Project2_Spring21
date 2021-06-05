# Comp304_Project2_Spring21
###Doğa Demirtürk
###İrem Şahin

To run the project with default values of n=4, p=0.75, q=5, t=3 and b=0.05, please write "make" to first compile the program.After this, please write "make test" to the terminal and simply watch the simulation. 

To run the project with some other arguments, please write "make" to first compile the program. After compilation, you can run the program by writing "./main -n (and other arguments)". If you do not enter an argument, that argument's value will stay as it's default value(4 for n, 0.75 for p and so on).

This program has all the components that we were asked to implement. It successfully executes part 1 and part 2 for all of the questions and commentators. It successfully executes breaking news sections too and interrupts the commentator if needed.

As it can be seen from sample_run_1.txt, sample_run_2.txt, sample_run_1.png and sample_run_2.png, logging part is also implemented as it was asked in the pdf.

In addition to given pthread sleep code, this program uses another sleep function for breaking news handling. The only difference in this function is that it uses timedwait to be able to successfully interrupt commentator threads.