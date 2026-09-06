#define DATA_LEN 0
#define THRESHOLD 0


typedef struct {
    float acc[3];
} imu_data_t;

int state;

int trigger_index;
float data[DATA_LEN];
void main() {   

    for (int i = 1; i < DATA_LEN-2; i++) {
        if (data[i] >= THRESHOLD && data[i-1] < data[i] && data[i] < data[i+1]) {
            trigger_index = i;
        }
        break;


}

