# pod_all

Core code for https://github.com/sec-bit/zkPoD-lib  

### ��ȡ����
1����libsnark���벢��ȡ������ע�ⲻҪ�ݹ���ȡ  
git submodule init && git submodule update
cd depends/libsnark  
git submodule init && git submodule update

2������pod_core��pod_setup��pod_publish��linux or osx��  
cd pod_core  
make  

cd ../pod_setup  
make  

cd ../pod_publish  
make  

����õĴ�����linux/binĿ¼�¡�  

3������pod_publish����һ���ļ�  
cd linux/bin  
./pod_publish -m table -f test100000.csv -o table_data -t csv -k 0 1  

4������pod_core  
cd linux/bin  
./pod_core -m table -a atomic_swap_pod_vc -p table_data -o table_output --demand_ranges 1-10  


### ���� (windows + msvc2019)��  
ֱ����msvc2019��pod_all.sln��  
