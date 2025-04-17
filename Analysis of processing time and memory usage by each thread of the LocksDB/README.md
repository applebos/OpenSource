# OpenSource

1) Analysis of processing time and memory usage by each thread of the LocksDB

실험 환경<br/>
CPU: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz<br/>
코어 수: 4<br/>
쓰레드 수: 4 (각 코어당 1개의 쓰레드)<br/>
운영체제: 리눅스 (64-bit)<br/>
가상화: KVM 하이퍼바이저를 통한 완전 가상화<br/>
CPU 캐시:
- L1 캐시: 128 KiB (각 코어당 1개)
- L2 캐시: 1 MiB (각 코어당 1개)
- L3 캐시: 48 MiB (전체 코어 공유)


한 번 실행
![8코어 16기가](https://github.com/user-attachments/assets/96729766-a052-4fb9-86b4-c7e9e58d8fd0)

10번 실행 후 평균
![평균(8thread)](https://github.com/user-attachments/assets/202eff01-a84d-4259-8ca8-3c04bad988af)

16thread까지 10번 실행 후 평균
![평균(16thread)](https://github.com/user-attachments/assets/5019f740-73f6-4dbb-ae44-ba58a8ab7150)
