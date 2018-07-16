#include <stdio.h>

#define TRIGGER_FILE "/sys/class/leds/beaglebone:green:usr0/trigger"

int main() {
	FILE *pLedTriggerFile = fopen(TRIGGER_FILE, "w");

	if (pLedTriggerFile == NULL) {
		printf("ERROR OPENING %s.", TRIGGER_FILE);
	}

	int charWritten = fprintf(pLedTriggerFile, "none");
	if (charWritten <= 0) {
		printf("ERROR WRITING DATA");
	}

	fclose(pLedTriggerFile);

	return 0;
}