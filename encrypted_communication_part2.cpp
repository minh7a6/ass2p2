#include <Arduino.h>


uint32_t gcd(uint32_t a, uint32_t b);
int32_t reduce_mod(int32_t x, uint32_t m);
uint32_t find_d(uint32_t e, uint32_t phi);
bool checkprime(uint32_t n);
uint32_t randnum(int bits);
uint32_t randprime(int bits);
bool wait_on_serial3(uint8_t nbytes, long timeout );
void uint32_to_serial3(uint32_t num);
uint32_t uint32_from_serial3();
void handshake_server(uint32_t skey, uint32_t smod, uint32_t &ckey, uint32_t &cmod);
void handshake_client(uint32_t ckey, uint32_t cmod, uint32_t &skey, uint32_t &smod);
void run(uint32_t d, uint32_t n, uint32_t e, uint32_t m);


uint32_t gcd(uint32_t a, uint32_t b) {
	while(b > 0) {
		a = a%b;
		uint32_t k = b;
		b = a;
		a = k;
	}
	return a;
}

int32_t reduce_mod(int32_t x, uint32_t m) {
	if(x >= 0) {
		return x%m;
	}
	else {
		int32_t z = (-x)/m + 1;
		x = x + z*m;
		return x % m;
	}
}

uint32_t find_d(uint32_t e, uint32_t phi) {
	int32_t r[40];
	int32_t s[40];
	int32_t t[40];
	r[0] = e; r[1] = phi;
	s[0] = 1; s[1] = 0;
	t[0] = 0; t[1] = 1;
	uint32_t i = 1;
	while(r[i] > 0) {
		int32_t q = r[i-1]/r[i];   // integer division
		r[i+1] = r[i-1] - q*r[i];  // same as r[i-1] mod r[i]
		s[i+1] = s[i-1] - q*s[i];
		t[i+1] = t[i-1] - q*t[i];
		i++;
	}
	int32_t d = s[i-1];
	d = reduce_mod(d,phi);
	return d;
}

bool checkprime(uint32_t n) { 
    if (n == 1) {
        return false; 
    } 
        
    else if (n == 2 || n == 3)  {
        return true; 
    }
  
    else if (n%2 == 0 || n%3 == 0) {
        return false; 
    }
  
    for (uint32_t i = 5; i*i <= n; i = i+6) {
        if (n%i == 0 || n%(i+2) == 0) {
           return false; 
        }
    }
  
    return true; 
} 

uint32_t randnum(int bits) {
	uint32_t result = 0;
	for (int i = 0; i < bits; i++) {
		result |= (((uint32_t)analogRead(A1) & 1) << i);
		delay(5);
	}
	return result;
}

uint32_t randprime(int bits) {
	uint32_t result = randnum(bits);
	result = result + (1ul << bits); // add 2^bits
	while(checkprime(result) != true) {
		result++;
	}
	return result;
}

bool wait_on_serial3(uint8_t nbytes, long timeout) {
	unsigned long deadline = millis() + timeout;// wraparound  not a problem
	while (Serial3.available() < nbytes && (timeout < 0 ||  millis() < deadline)){
		delay (1); // be nice , no busy  loop
	}
	return  Serial3.available() >= nbytes;
}

/* 
    Description: Push all the data through Serial3.

    Arguments:
        num(uint32_t): Input after all Calculation

    Returns:
        None
*/

void uint32_to_serial3(uint32_t num) {
	Serial3.write((char) (num  >> 0));
	Serial3.write((char) (num  >> 8));
	Serial3.write((char) (num  >> 16));
	Serial3.write((char) (num  >> 24));
}

/* 
    Description: Read all the bits from Serial3 Stream.

    Arguments:
        None

    Returns:
        num in uint32_t after combining all 4 bytes.
*/


uint32_t uint32_from_serial3() {
	uint32_t num = 0; 
	num = num | ((uint32_t) Serial3.read()) << 0;
	num = num | ((uint32_t) Serial3.read()) << 8;
	num = num | ((uint32_t) Serial3.read()) << 16;
	num = num | ((uint32_t) Serial3.read()) << 24;
	return num;
}

void generate_key(uint32_t &n, uint32_t &e, uint32_t &d) {
	uint32_t p = randprime(14);
	uint32_t q = randprime(15);
	n = p*q;
	uint32_t phi = (p-1)*(q-1);
	e = randnum(15);
	while (gcd(e,phi) != 1) {
		e = randnum(15);
	}
	d = find_d(e, phi);
}

/* 
    Description: Multipication Mod without overflowing

    Arguments:
        a(uint32_t), b(uint32_t), m(uint32_t): two factors with a modular number

    Returns:
        value after calculation
*/

uint32_t mulmod (uint32_t a, uint32_t b, uint32_t m) {
	uint32_t ans = 0;
	while (a != 0) { // if a is not below 0, continue calculation
		if(a & 1 == 1) { // got an a not 0, start calculation for modulation
			ans = (ans + b) % m;
		}
		a >>= 1;
		b = (b << 1) % m;		
	}
	return ans;
}

/* 
    Description: Fast Power Modulation

    Arguments:
        x(uint32_t), pow(uint32_t), m(uint32_t): the input value with power to value and modular number

    Returns:
        ans for values after calculation
*/

uint32_t powmod(uint32_t x, uint32_t pow, uint32_t m) {
  uint32_t ans = 1;
  uint32_t pow_x = x;

  while (pow > 0) {
    if (pow & 1 == 1) {
        ans = mulmod(ans,pow_x,m);
    }

    pow_x = mulmod(pow_x,pow_x,m);
    pow >>= 1; 
  }

  return ans;
}



/* 
    Description: Run the main function for either Server or Clients depends on numbers and keys

    Arguments:
        d(uint32_t), n(uint32_t), e(uint32_t), m(uint32_t): all the keys and number configured in main loop

    Returns:
        None.
*/
void handshake_server(uint32_t skey, uint32_t smod, uint32_t &ckey, uint32_t &cmod) {
	bool stat = true;
	Serial.println("waiting for keys");
	while(stat) {
		if(wait_on_serial3(9,1000)){
			if(Serial3.read() == 'C') {
				ckey = uint32_from_serial3();
				cmod = uint32_from_serial3();
				Serial3.write('A');
				uint32_to_serial3(skey);
				uint32_to_serial3(smod);
				if(wait_on_serial3(1,1000)){
					if(Serial3.read() == 'A'){
						stat = false;
					}
					else if(Serial3.read() == 'C')
						if(wait_on_serial3(8,1000)) {
							ckey = uint32_from_serial3();
							cmod = uint32_from_serial3();
						}
						else {
							//uint32_t ckey, cmod;
							handshake_server(skey, smod, ckey, cmod);
						}
				}
				else {
					//uint32_t ckey, cmod;
					handshake_server(skey, smod, ckey, cmod);
				}
			}
		}
		else {
			handshake_server(skey, smod, ckey, cmod);
		}
	}
	Serial.print("ckey: ");
	Serial.println(ckey);
	Serial.print("cmod: ");
	Serial.println(cmod);
}

void handshake_client(uint32_t ckey, uint32_t cmod, uint32_t &skey, uint32_t &smod) {
	Serial.println("requesting for keys");
	bool stat = true;
	while(stat) {
		Serial3.write('C');
		uint32_to_serial3(ckey);
		uint32_to_serial3(cmod);
		if(wait_on_serial3(9,1000)) {
			uint32_t ack = Serial3.read();
			if (ack == 'A') {
				skey = uint32_from_serial3();
				smod = uint32_from_serial3();
				Serial.print("skey: ");
				Serial.println(skey);
				Serial.print("smod: ");
				Serial.println(smod);
				Serial3.write('A');
				stat = false;
			}
		}
		else {
			handshake_client(ckey, cmod, skey, smod);
		}
	}
}

void run(uint32_t d, uint32_t n, uint32_t e, uint32_t m) {
	if (Serial.available()) { // if there is input from terminal
		uint32_t message = Serial.read();
		if (message == '\r') { // if it is carriage return
			Serial.write(message); // output back to terminal
			Serial.write('\n');	// with newline
			uint32_t encryptnewline = powmod('\n', e, m); // run the encryption for newline
			uint32_t encrypt = powmod(message, e, m); // run the encryption for actual message
			uint32_to_serial3(encrypt); // push the message via serial3
			uint32_to_serial3(encryptnewline);
		}
		else { // otherwise only encrypt the message
			Serial.print(char(message)); 
			uint32_t encrypt = powmod(message, e, m); 
			uint32_to_serial3(encrypt);
		}
	}
	if (Serial3.available() >= 4) { // if there is upcoming stream from serial3, waiting for 4 bytes
		uint32_t message = uint32_from_serial3();
		uint32_t decrypt = powmod(message, d, n); // decrypt the message
		Serial.write(decrypt); // output to terminal
	}
}

void setup() {
	init();
	Serial.begin(9600);
	Serial3.begin(9600);
	pinMode(13, INPUT_PULLUP);
}

int main() {
	setup();
	bool is_server = digitalRead(13);
	uint32_t mod, publickey, privatekey;
	generate_key(mod, publickey, privatekey);
	if (is_server) {
		Serial.println("Welcome to Arduino Chat!");
		Serial.println("Server");
		Serial.println("generated key:");
		Serial.print("mod: ");
		Serial.println(mod);
		Serial.print("publickey: ");
		Serial.println(publickey);
		Serial.print("privatekey: ");
		Serial.println(privatekey);
		uint32_t ckey,cmod;
		handshake_server(publickey,mod,ckey,cmod);
		uint32_t d = privatekey;
		uint32_t n = mod;
		uint32_t e = ckey;
		uint32_t m = cmod;	
		while(1) {
			run(d,n,e,m);
		}
	}
	else {
		Serial.println("Welcome to Arduino Chat!");
		Serial.println("Client");
		Serial.println("generated key:");
		Serial.print("mod: ");
		Serial.println(mod);
		Serial.print("publickey: ");
		Serial.println(publickey);
		Serial.print("privatekey: ");
		Serial.println(privatekey);
		uint32_t skey,smod;
		handshake_client(publickey, mod, skey, smod);
		uint32_t d = privatekey;
		uint32_t n = mod;
		uint32_t e = skey;
		uint32_t m = smod;	
		while(1) {
			run(d,n,e,m);
		}
	}
	Serial.flush();
	Serial3.flush();
	return 0;
}
