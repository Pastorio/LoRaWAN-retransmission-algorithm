#include <iostream>
#include <stdint.h>
#include <string>
#include <time.h>
#include<sstream>  
#include <list>

using namespace std;

struct mensagem {
    int fcnt;
    int dado;
};

int main() {
    std::list<mensagem> registro_mensagem;

    int dado = 12;

    struct mensagem m1 = {1, dado};
    cout << m1.dado << endl;

    registro_mensagem.push_back(m1);

    for (const mensagem & val : registro_mensagem){
        cout << val.dado << endl;
        cout << " " << endl;
    }

    return 0;
}


// int main()
// {
//     srand (time(NULL)); // initialize random seed
    
//     string mensagem_padrao = "Mensagem ";

//     int dado = rand() % 100 + 1;
    
//     string result = mensagem_padrao + to_string(dado);

//     std::cout << result << std::endl;

//     const uint8_t* mensagem_enviar = reinterpret_cast<const uint8_t*>(result.c_str());
    
//     std::cout <<  mensagem_enviar << std::endl;

// }