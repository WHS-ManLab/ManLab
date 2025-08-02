# ManLab

<p align="center">
  <img src="로고.jpg" alt="ManLab 로고" width="300">
</p>

<p align="center">
  ManLab은 Linux 환경에서 동작하는 미니 보안제품입니다.
</p>
<p align="center">
  ManLab is a mini security product for Linux (Ubuntu 20.04).
</p>

---

**개요**

ManLab은 리눅스 시스템의 기본적인 보안 강화를 목표로 하는 미니 보안제품입니다.

---

**주요 기능**

* **파일 무결성 모니터링(FIM)**
  - 수동 검사 : 베이스라인(해시값, 메타데이터)을 생성하고 이를 이용하여 파일의 무결성을 검증합니다.
  - 실시간 검사 : 파일 생성, 삭제, 수정, 이동, 권한 변경 등의 이벤트를 감지합니다.

  
* **시그니처 분석 악성코드 탐지**
  - 악성코드 해시 DB, YARA 룰셋 기반 패턴 매칭을 통해 파일의 악성 여부를 판단합니다.
  - 악성코드로 의심되는 파일을 격리하고, 시스템 알림을 제공합니다.
 
* **로그분석 악성행위 탐지**
  - Rsyslog와 Auditd를 이용하여 리눅스 시스템 로그를 실시간으로 수집합니다.
  - 사전에 정의된 악성 행위 룰과 매칭되는 로그를 탐지하면 시스템 알림을 제공합니다.
 
* **리포트 생성 및 발송**
  - 사용자의 설정에 따라 html 형식의 정기 리포트를 생성합니다.
  - 검사 결과, 악성 행위 매칭 결과 등의 정보를 리포트에서 확인할 수 있습니다.

---

**주요 디렉토리 및 파일**

```
/root/ManLab/conf/           # 설정 파일 디렉토리
/root/ManLab/rules/          # YARA 룰 파일
/root/ManLab/quarantine/     # 격리된 파일 저장소
/root/ManLab/logs/manlab.log # 시스템 로그 파일
```

---

**설치 방법**

1. ManLab 레포지토리 `git clone`
2. `make clean && make` 실행
3. **sudo** 혹은 **root** 권한으로 `./install.sh` 실행
4. `지금 ManLab 서비스를 실행하시겠습니까? [Y/n]` 출력 시
     - `Y` + **Enter** 또는 **Enter** → 즉시 실행
     - `n` + **Enter** → 추후 `sudo systemctl start manlab-init.service` 명령어로 실행

---

**사용 방법**

- root 권한으로 명령어를 입력해주세요.
```
ManLab malscan               # 악성코드 수동 검사 수행
ManLab restore <filename>    # 격리된 파일 복원
check_malreport              # 최근 악성코드 검사 리포트 20개 출력
ManLab baseline              # 무결성 기준(Baseline) 수집
ManLab integscan             # 무결성 검사 수행
ManLab check_baseline        # 수집된 Baseline 출력
ManLab check_integscan       # 무결성 검사 결과 출력
ManLab man                   # 도움말 출력
```
- 주요 설정 파일은 다음과 같습니다. 자세한 형식은 각 INI 파일 내 주석을 참고하세요.
```
FIMConfig.ini                # 실시간 파일 변경 감시 설정
FIMIntegScan.ini             # 무결성 검사 대상/제외 설정
MalScanConfig.ini            # 악성코드 검사 대상 및 제외, 용량 제한
ManLabconf.ini               # 재부팅 시 프로그램 자동 실행 설정
ReportConfig.ini             # 리포트 주기 및 이메일 전송 설정
ScanSchedul.ini              # 예약 검사 시간 및 주기 설정
```

---

**삭제 방법**

1. ManLab 디렉토리에서 **sudo** 혹은 **root** 권한으로 `./uninstall.sh` 실행
2. `삭제 방식을 선택해 주세요:` 출력 시
   - `1` + **Enter** → 전체 삭제
   - `2` + **Enter** → 부분 삭제 (리포트, 로그, DB 유지)


---

