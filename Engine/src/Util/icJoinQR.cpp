#include <utility>
#include "Util/icJoinQR.h"
#include <vector>
#include <algorithm>
#include <cstdlib>
namespace {
unsigned char multiply(unsigned char a,unsigned char b){unsigned result=0,x=a;for(int i=0;i<8;++i){if(b&1)result^=x;b>>=1;x<<=1;if(x&256)x^=0x11d;}return (unsigned char)result;}
}
bool icJoinQR::Encode(const std::string& text){
 dark={};if(text.empty()||text.size()>53)return false;
 std::vector<bool> bits;auto append=[&](unsigned value,int count){for(int i=count-1;i>=0;--i)bits.push_back((value>>i)&1);};
 append(4,4);append((unsigned)text.size(),8);for(unsigned char ch:text)append(ch,8);
 append(0,std::min(4,440-(int)bits.size()));while(bits.size()%8)bits.push_back(false);
 std::vector<unsigned char> words;for(unsigned i=0;i<bits.size();i+=8){unsigned value=0;for(int j=0;j<8;++j)value=value*2+bits[i+j];words.push_back((unsigned char)value);}
 for(unsigned pad=0;words.size()<55;++pad)words.push_back(pad%2?0x11:0xec);
 // Generator polynomial with roots alpha^0..alpha^14 in GF(256).
 std::vector<unsigned char> generator(1,1);unsigned char root=1;
 for(int degree=0;degree<15;++degree){std::vector<unsigned char> next(generator.size()+1);for(unsigned j=0;j<generator.size();++j){next[j]^=generator[j];next[j+1]^=multiply(generator[j],root);}generator=next;root=multiply(root,2);}
 auto remainder=words;remainder.resize(70);for(int i=0;i<55;++i){unsigned char factor=remainder[i];for(unsigned j=0;j<generator.size();++j)remainder[i+j]^=multiply(generator[j],factor);}
 words.insert(words.end(),remainder.begin()+55,remainder.end());
 std::array<std::array<bool,Size>,Size> function{};
 auto set=[&](int x,int y,bool value){if(x>=0&&x<Size&&y>=0&&y<Size){dark[y][x]=value;function[y][x]=true;}};
 for(auto center:{std::pair<int,int>(3,3),{25,3},{3,25}})for(int y=-4;y<=4;++y)for(int x=-4;x<=4;++x){int distance=std::max(std::abs(x),std::abs(y));set(center.first+x,center.second+y,distance!=2&&distance!=4);}
 for(int i=0;i<Size;++i){if(!function[6][i])set(i,6,i%2==0);if(!function[i][6])set(6,i,i%2==0);}
 for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x)set(22+x,22+y,std::max(std::abs(x),std::abs(y))!=1);
 unsigned format=8,check=format<<10;for(int bit=14;bit>=10;--bit)if(check&(1u<<bit))check^=0x537u<<(bit-10);format=((format<<10)|check)^0x5412;
 auto f=[&](int bit){return ((format>>bit)&1)!=0;};
 for(int i=0;i<6;++i)set(8,i,f(i));set(8,7,f(6));set(8,8,f(7));set(7,8,f(8));for(int i=9;i<15;++i)set(14-i,8,f(i));
 for(int i=0;i<8;++i)set(Size-1-i,8,f(i));for(int i=8;i<15;++i)set(8,Size-15+i,f(i));set(8,Size-8,true);
 unsigned index=0;
 for(int right=Size-1;right>=1;right-=2){if(right==6)right=5;bool upward=((right+1)&2)==0;
  for(int row=0;row<Size;++row){int y=upward?Size-1-row:row;for(int offset=0;offset<2;++offset){int x=right-offset;if(function[y][x])continue;bool value=index<560?((words[index/8]>>(7-index%8))&1)!=0:false;++index;dark[y][x]=value^((x+y)%2==0);}}
 }
 return index==567;
}
