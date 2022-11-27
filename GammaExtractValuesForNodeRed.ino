//#define _SMARTDEBUG

#ifdef _SMARTDEBUG
#define DEBUG_PRINTLN(txt) Serial.println(txt)
#define DEBUG_PRINTLN_VALUE(txt, val) \
    Serial.print(txt);                \
    Serial.println(val)
#define DEBUG_WAIT(millis) delay(millis)
#else
#define DEBUG_PRINTLN(txt)
#define DEBUG_PRINTLN_VALUE(txt, val)
#define DEBUG_WAIT(millis)
#endif

const int REC_ARR_LEN = 4;

struct datagram_t
{
    byte receiver[REC_ARR_LEN];
    int receiverIndex;
    byte fill1;
    byte receiver1;
    byte fill2;
    byte receiver2;
    byte start;
    byte sender;
    byte rw;
    byte length;
    byte function;
    byte data[128];
    int dataIndex;
    byte crc1;
    byte crc2;
    byte end;
    byte post;
};

typedef struct datagram_t datagram_t;

datagram_t inDatagram, outDatagram;

typedef enum
{
    undefined = 0,
    startDone = 1,
    senderDone = 2,
    rwDone = 3,
    lengthDone = 4,
    functionDone = 5,
    dataDone = 6,
    crc1Done = 7,
    crc2Done = 8,
    endDone = 9,
    datagramDone = 10,
} parserState_t;

const int START_BYTE = 130; //Hex 82
const int SENDER_CONTROL_CENTER = 16; //Hex 10 
 parserState_t parserState = undefined;
boolean ignoreNextByte = false;

void setup()
{

//Serial für die Kommunikation mit dem PC
    Serial.begin(9600);
    while (!Serial)
    {
        ; // wait for serial port to connect.
    }

//Serial3 für die Kommunikation mit dem Regelgerät der Heizung
    Serial3.begin(9600);
    while (!Serial3)
    {
        ; // wait for serial port to connect.
    }

}

void loop()
{

    byte receivedByte;

    DEBUG_WAIT(1500);

    if (Serial3.available())
    {

        receivedByte = Serial3.read();

        //jedes zweite Byte enthält Datenmüll; liegt vermutlich daran, dass wir
        //Serial3 eigentlich auf "1 Parity-Bit, welches zu ignorieren ist" setzen
        //müssten - diese Konfiguration steht jedoch leider nicht zur Verfügung
        if (ignoreNextByte == true)
        {
            ignoreNextByte = false;
            return;
        }
        else
        {
            ignoreNextByte = true;
        }

        DEBUG_PRINTLN_VALUE("receivedByte: ", receivedByte);

        //Parsen des Datagramms
        datagramParse(receivedByte, &(parserState), &inDatagram);
        DEBUG_PRINTLN_VALUE("parserState NACH Parsing: ", parserState);
        DEBUG_PRINTLN("----------------------");

        if (parserState == datagramDone) //Datagramm fertig
        {
            //Verarbeitung des Datagramms
            datagramProcess(&inDatagram);

            //Initialisierung Parser und inDatagram
            parserState = undefined;
            inDatagram = {};
        }

        }
    }

    void datagramProcess(datagram_t * inDatagram) {

    byte istWarmwasserTemp = 0;
    byte istAussenTemp = 0;
    byte istKesselTemp = 0;
    byte istVorlaufTemp = 0;
    byte brennerAn = 0;
    byte istRaumTemp = 0;
    byte sollRaumTemp = 0;
    byte sollWarmwasserTemp = 0;


        //wir interessieren uns nur für Datagramme, die vom Regelgerät kommen
        if (inDatagram->sender == SENDER_CONTROL_CENTER) {

            switch (inDatagram->function) {
            case 1:
                break;              
            case 2:
                //IST-Warmwassertemperatur
                istWarmwasserTemp = inDatagram->data[6] / 2;
                Serial.print("IST-Temp. WW: ");
                Serial.println(istWarmwasserTemp);
                //IST-Vorlauftemperatur
                istVorlaufTemp = inDatagram->data[9] / 2;
                Serial.print("IST-Temp. Vorlauf: ");
                Serial.println(istVorlaufTemp);
                break;
            case 4:
                //IST-Außentemperatur
                istAussenTemp = (inDatagram->data[0] / 2) - 52;
                Serial.print("IST-Temp. Außen: ");
                Serial.println(istAussenTemp);
                //IST-Kesseltemperatur
                istKesselTemp = inDatagram->data[30] / 2;
                Serial.print("IST-Temp. Kessel: ");
                Serial.println(istKesselTemp);
                //Flag: Brenner an?
                brennerAn = inDatagram->data[17];
                Serial.print("IST-Flag Brenner an: ");
                if (brennerAn == 1) {
                  Serial.println("true");
                } else {
                  Serial.println("false");
                }
                break;
            case 5:
                break;
            case 6:
                //IST-Raumtemperatur
                istRaumTemp = inDatagram->data[0] / 2;
                Serial.print("IST-Temp. Raum: ");
                Serial.println(istRaumTemp);
                //SOLL-Raumtemperatur
                sollRaumTemp = inDatagram->data[2] / 2;
                Serial.print("SOLL-Temp. Raum: ");
                Serial.println(sollRaumTemp);
                //SOLL-Warmwassertemperatur
                sollWarmwasserTemp = inDatagram->data[8] / 2;
                Serial.print("SOLL-Temp. WW: ");
                Serial.println(sollWarmwasserTemp);
                break;
            }
        }
    }

    void datagramParse(const byte receivedByte,  parserState_t *parserState, datagram_t *inDatagram)
    {

        int dataLength = 0;

        switch (*parserState)
        {
        case startDone:
            inDatagram->sender = receivedByte;
            *parserState = senderDone;
            break;
        case senderDone:
            inDatagram->rw = receivedByte;
            *parserState = rwDone;
            break;
        case rwDone:
            inDatagram->length = receivedByte;
            *parserState = lengthDone;
            break;
        case lengthDone:
            inDatagram->function = receivedByte;
            *parserState = functionDone;
            break;
        case functionDone:
            inDatagram->data[inDatagram->dataIndex] = receivedByte;
            inDatagram->dataIndex++;
            DEBUG_PRINTLN_VALUE("inDatagram->length: ", inDatagram->length);
            if (inDatagram->dataIndex == inDatagram->length)
            {
                *parserState = dataDone;
            }
            break;
        case dataDone:
            inDatagram->crc1 = receivedByte;
            *parserState = crc1Done;
            break;
        case crc1Done:
            inDatagram->crc2 = receivedByte;
            *parserState = crc2Done;
            break;
        case crc2Done:
            inDatagram->end = receivedByte;

            //ein abschließendes "06" erwarten wir genau dann, wenn
            //-wir in fill2 bereits ein "06" hatten UND
            //-es sich NICHT um einen Schreibzugriff handelte
            if (inDatagram->fill2 == 6 && inDatagram->rw != 32)
            {
                *parserState = endDone;
            }
            else
            {
                *parserState = datagramDone;
            }
            break;
        case endDone:
            inDatagram->post = receivedByte;
            *parserState = datagramDone;
            break;
        default:
            if (receivedByte == START_BYTE)
            {
                inDatagram->start = receivedByte;
                receiversParse(inDatagram->receiver, &(inDatagram->fill1), &(inDatagram->receiver1), &(inDatagram->fill2), &(inDatagram->receiver2));
                *parserState = startDone;
            }
            else
            {
                if (inDatagram->receiverIndex < REC_ARR_LEN)
                {
                    inDatagram->receiver[inDatagram->receiverIndex] = receivedByte;
                    inDatagram->receiverIndex++;
                }
                else
                {
                    inDatagram->receiver[0] = inDatagram->receiver[1];
                    inDatagram->receiver[1] = inDatagram->receiver[2];
                    inDatagram->receiver[2] = inDatagram->receiver[3];
                    inDatagram->receiver[3] = receivedByte;
                }
            }
            break;
        }

    }

    void receiversParse(const byte receiverArray[], byte *fill1, byte *receiver1, byte *fill2, byte *receiver2)
    {

        //Prüfung Byte 4
        if (receiverArray[3] == 125 || receiverArray[3] == 33 || receiverArray[3] == 144 || receiverArray[3] == 255 || receiverArray[3] == 252)
        {
            *receiver2 = receiverArray[3];
            //nach 7D, FC und FF kommt nix mehr
            if (receiverArray[3] == 125 || receiverArray[3] == 255 || receiverArray[3] == 252)
            {
                return;
            }
        }
        else
        {
            return;
        }

        //Prüfung Byte 3
        if (receiverArray[2] == 6)
        {
            *fill2 = receiverArray[2];
        }
        else
        {
            return;
        }

        //Prüfung Byte 2
        if (receiverArray[1] == 33 || receiverArray[1] == 144)
        {
            *receiver1 = receiverArray[1];
        }
        else
        {
            return;
        }

        //Prüfung Byte 1
        if (receiverArray[0] == 6)
        {
            *fill1 = receiverArray[0];
        }
        else
        {
            return;
        }
    }
