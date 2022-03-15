# Malloc Lab porject

C 언어의 `malloc`과 `free`를 구현하는 프로젝트 입니다.


### 코드 실행 방법

```bash
# ubuntu 환경
sudo apt update
sudo apt install build-essential
sudo apt install gdb
sudo apt-get install gcc-multilib g++-multilib

git clone https://github.com/emplam27/Malloc-Lab-project.git
cd Malloc-Lab-project/malloc-mdriver

# mm.c 파일내용을 explicit-malloc.c, implicit-malloc(first-fit).c, implicit-malloc(next-fit).c로 수정
make re
```
