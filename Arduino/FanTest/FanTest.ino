
#define FanIA 6 //fan conrol input A
#define FanIB 5 //fan conrol input B

#define Fan_PWM FanIA //fan PWM speed
#define Fan_DIR FanIB //fan direction
// the actual values for "fast" and "slow" depend on the motor
#define PWM_SLOW 50 // arbitrary slow speed PWM duty cycle
#define PWM_FAST 200 // arbitrary fast speed PWM duty cycle
#define PWM_FULL 255 // 255 is the new over 9000

int currentFanSpeed = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting up");
  
  pinMode( Fan_PWM, OUTPUT );
  pinMode( Fan_DIR, OUTPUT );
  stopFan();
}

void loop() {
  // put your main code here, to run repeatedly:
//  fanSpeed(PWM_FULL);
//  delay(5000);
  stopFan();
  delay(5000);
}

void stopFan()
{
  currentFanSpeed = 0;
  Serial.println("Stopping fan");
  digitalWrite( Fan_PWM, LOW );
  digitalWrite( Fan_DIR, LOW );
}

void fanSpeed(int speed)
{
  Serial.println("Setting fan speed " + String(speed));
  currentFanSpeed = speed;
  // set the motor speed and direction
  digitalWrite( Fan_DIR, HIGH );
  analogWrite( Fan_PWM, 255-speed ); // PWM speed = fast   
}
