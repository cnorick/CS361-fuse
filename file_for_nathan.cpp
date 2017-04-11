#include <iostream>
#include <string>
#include <list>
using namespace std; 

list <string> Split(string abs_path){
    list <string> retList;
    int found, last;
    string str = "";

    for(int i = 0; i < abs_path.size(); i++){
        if(abs_path[i] == '/'){
            if(str != "") retList.push_back(str);
            str = "";
        }
        str += abs_path[i];    
    }
    if(str != "") retList.push_back(str);

    return retList;
}


int main(){
    string str, s1, s2;
    string str1 = "/four/three/two/one";
    string str2 = "/one/two/three/four/";
    list <string> l1 = Split(str1);
    list <string> l2 = Split(str2);
    
    while(!l1.empty()){
        s1 = l1.front();
        s2 = l2.front();
        l1.pop_front();
        l2.pop_front();
        cout << s1 << endl;
        cout << s2 << endl;
    }


}
