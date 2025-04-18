# OpenSource

1) Analysis of processing time and memory usage by each thread of the LocksDB

CPU 사양:
- 모델: AMD Ryzen 7 7800X3D 8-Core Processor
- cCPU 아키텍처: x86_64 (64-bit)
- 코어 수: 8 (1스레드당 1개의 스레드)
- CPU 속도 및 성능:
  - BogoMIPS: 8384.26
  - 다양한 고급 명령어 집합 지원 (AVX, AVX2, AES, FMA 등)
캐시 정보:
L1d Cache: 256 KiB (각각 8개 인스턴스)
L1i Cache: 256 KiB (각각 8개 인스턴스)
L2 Cache: 8 MiB (각각 8개 인스턴스)
L3 Cache: 768 MiB (8개 인스턴스)
가상화 환경: KVM (Full virtualization)


한 번 실행
![8코어 16기가](https://github.com/user-attachments/assets/96729766-a052-4fb9-86b4-c7e9e58d8fd0)

10번 실행 후 평균
![평균(8thread)](https://github.com/user-attachments/assets/202eff01-a84d-4259-8ca8-3c04bad988af)

16thread까지 10번 실행 후 평균
![평균(16thread)](https://github.com/user-attachments/assets/5019f740-73f6-4dbb-ae44-ba58a8ab7150)
