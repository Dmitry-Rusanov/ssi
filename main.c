#include <avr/io.h>
#include <avr/interrupt.h>

// ����������� ����� � ����������
#define CLOCK_PIN PB0  // D8 �� Arduino Uno
#define DATA_PIN  PB1  // D9 �� Arduino Uno
#define BAUD 9600      // �������� UART
#define F_CPU 16000000UL  // ������� ����������������

// ���������� ����������
volatile uint32_t ssi_data = 0;     // ������ � SSI
volatile uint8_t ssi_bit_count = 0; // ������� ����� ��� SSI
volatile uint8_t ssi_state = 0;     // ��������� ������ SSI (0 - ��������, 1 - ������)
volatile uint8_t output_flag = 0;   // ���� ��� ������ ������

// ������������� UART
void uart_init()
{
	UBRR0 = F_CPU / 16 / BAUD - 1;
	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// �������� ������� ����� UART
void uart_send_char(char c)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

// �������� ������ ����� UART
void uart_send_string(const char* str)
{
	while (*str)
	{
		uart_send_char(*str++);
	}
}

// �������� ����� � ��������� ������ (� ��)
void uart_send_float(float num)
{
	int integer_part = (int)num;
	char buffer[16];
	int i = 0;
	
	if (integer_part == 0)
	{
		uart_send_char('0');
	}
	else
	{
		while (integer_part > 0) 
		{
			buffer[i++] = (integer_part % 10) + '0';
			integer_part /= 10;
		}
		for (int j = i - 1; j >= 0; j--)
		{
			uart_send_char(buffer[j]);
		}
	}
	
	uart_send_char('.');
	float decimal_part = num - (int)num;
	for (int j = 0; j < 3; j++)
	{
		decimal_part *= 10;
		int digit = (int)decimal_part;
		uart_send_char(digit + '0');
		decimal_part -= digit;
	}
}

// ������������� Timer1
void timer1_init()
{
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS11); // CTC, ������������ 8
	OCR1A = 10;                          // 5 ���
	TIMSK1 = (1 << OCIE1A);
	TCNT1 = 0;
}

// ������������� ����� SSI
void initSSI() 
{
	DDRB |= (1 << CLOCK_PIN);   // Clock ��� �����
	DDRB &= ~(1 << DATA_PIN);   // Data ��� ����
	PORTB |= (1 << DATA_PIN);   // �������� Data � VCC
	PORTB &= ~(1 << CLOCK_PIN); // Clock ���������� LOW
}

// ���������� ���������� Timer1
ISR(TIMER1_COMPA_vect)
{
	if (ssi_state == 0) return;

	if (ssi_bit_count == 0)
	{
		PORTB &= ~(1 << CLOCK_PIN); // ��������� LOW
	}
	else 
	if (ssi_bit_count <= 50)
	{
		if (ssi_bit_count % 2 == 1)
		{
			PORTB |= (1 << CLOCK_PIN);  // Clock HIGH
		}
		else
		{
			PORTB &= ~(1 << CLOCK_PIN); // Clock LOW
			ssi_data <<= 1;
			if (PINB & (1 << DATA_PIN)) ssi_data |= 1;
		}
	}

	ssi_bit_count++;
	if (ssi_bit_count > 50)
	{
		ssi_state = 0;
		ssi_bit_count = 0;
		output_flag = 1;
	}
}

int main(void) {
	initSSI();
	uart_init();
	timer1_init();
	sei();

	uart_send_string("System Started\n"); // �������� UART

	uint32_t poll_counter = 0;

	while (1) 
	{
		if (ssi_state == 0 && poll_counter >= 100000) {
			ssi_data = 0;
			ssi_state = 1;
			poll_counter = 0;
			//uart_send_string("Reading SSI\n");
		}
		poll_counter++;

		if (output_flag)
		{
			float position_mm = (ssi_data & 0xFFFFFF) * 0.005; // 5 ��� = 0.005 ��
			uart_send_string("Position: ");
			uart_send_float(position_mm);
			uart_send_string(" mm\n");
			output_flag = 0;
		}
	}
}