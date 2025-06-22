<div align="center">

# IOT_PROJECT_Team1 : DOT RPG

<!-- logo -->
<img src="https://github.com/user-attachments/assets/fdf3cb8d-2168-45a0-860f-699285c8dd3f" width="300"/>

#### Archro-EM Kit를 활용한 도트 RPG 게임

</div> 

## 📝 Preview
<div align="center">
<br>
  
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/iLiySJ9f4mc/0.jpg)](https://www.youtube.com/watch?v=iLiySJ9f4mc) <br>
시연 영상
(위 이미지 클릭 시 유튜브로 이동합니다.)
<br>
</div>


## 📝Flow Chart
<br>
<div align="center">
<img src="https://github.com/user-attachments/assets/cccd1f31-e6e4-486e-aebf-43a239167913"/>
<br>
</div>

<br>

## 📝 Overview
Archro-EM kit를 활용한 RPG 게임 프로젝트입니다. (25.05.29 ~ 25.06.23)
- 이 프로젝트에서는 Raspberry Pi 플랫폼에서 Dot Matrix를 활용하여 RPG 게임을 제작하였습니다.
- Dot Matrix와 Push Button을 주 입출력 장치로서 활용하며 긴장감 있는 RPG 게임의 요소를 최대한 녹여보았습니다.


## 📝Features
- <strong>Push Button 입력을 통한 플레이어 이동 및 공격 기능</strong><br><br>
- <strong>Dot Matrix 실시간 출력 제어 기능</strong><br>
  - 장애물 상 → 하 / 하 → 상 /  좌 → 우 / 우 → 좌 방향으로 생성 기능
  - 장애물은 랜덤한 행/열에서 생성
  - 플레이어 표기 및 장애물 충돌 감지 기능
  - 함정 발동 및 보스전 입장 시 Dot Matrix + Buzzer 연출 기능
  - 보스 격파 및 플레이어 사망 시 Dot Matrix 연출<br><br>
- <strong>미로 구현 및 미로 내 함정 구현</strong>
  - 함정 발동 시 dot_challenge.c 실행 및 일정 시간 생존해야 해당 맵 탈출 가능
  - 함정 내부의 장애물은 일정 시간마다 생성되는 방향이 바뀜
  - 일정 시간 생존 시 원래 있던 위치로 복귀<br><br>
- <strong>보스전</strong>
  - 보스 AI : 3초마다 랜덤한 패턴의 특수 공격 발동
  - LED를 통한 보스와 플레이어의 체력 표기
    - 공격에 피격 시 체력 -1 (LED 1개 off)    

<br>

## 🔹Roles in Charge

<div sytle="overflow:hidden;">
<table>
  <tr>
    <td colspan="3" align="center"><strong>Team 1</strong></td>
  </tr>
  <tr>
    <td align="center">
      <a href="https://github.com/Hooie"><img src="https://avatars.githubusercontent.com/u/67465736?v=4" width="150px;" alt="이태호"/><br/><sub><b>이태호</b></sub></a>
    </td>
    <td align="center">
      <a href="https://github.com/ParkSehyeon1009"><img src="https://avatars.githubusercontent.com/u/138639429?v=4" width="150px" alt="박세현"/><br/><sub><b>박세현</b></sub></a>
    </td>
    <td align="center">
      <a href="https://github.com/pupulinlin"><img src="https://avatars.githubusercontent.com/u/108519362?v=4" width="150px" alt="김도건"/><br/><sub><b>김도건</b></sub></a>
    </td>
  </tr>
</table>
> 이태호 : PM , H/W , S/W , Presentation <br><br>
> 박세현 : H/W , S/W <br><br>
> 김도건 : QA , S/W, Documentation  <br><br>
</div>
<br>
