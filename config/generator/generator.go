package config_generator

import (
	rand2 "crypto/rand"
	"fmt"
	"math/rand"
	"strings"
	"time"

	"github.com/google/uuid"
)

type RandomGeneratedValue string

type RandomGeneratedValues struct {
	Hostname     RandomGeneratedValue `json:"hostname"`
	ProcessorID  RandomGeneratedValue `json:"processor_id"`
	UUID         RandomGeneratedValue `json:"uuid"`
	Motherboard  RandomGeneratedValue `json:"motherboard"`
	BIOS         RandomGeneratedValue `json:"bios"`
	MachineGUID  RandomGeneratedValue `json:"machineguid"`
	InstallDate  RandomGeneratedValue `json:"installdate"`
	OSSerial     RandomGeneratedValue `json:"osserial"`
	SID          RandomGeneratedValue `json:"sid"`
	MAC          RandomGeneratedValue `json:"mac"`
	WLANGUID     RandomGeneratedValue `json:"wlanguid"`
	BSSID        RandomGeneratedValue `json:"bssid"`
	DiskSerial   RandomGeneratedValue `json:"diskserial"`
	VolumeSerial RandomGeneratedValue `json:"volumeserial"`
	DiskModel    RandomGeneratedValue `json:"diskmodel"`
}

func GenerateRandomValues() RandomGeneratedValues {
	source := rand.NewSource(time.Now().UnixNano())
	r := rand.New(source)

	values := RandomGeneratedValues{}

	values.Hostname = RandomGeneratedValue(fmt.Sprintf("PC-%06d", r.Intn(1000000)))

	values.ProcessorID = RandomGeneratedValue(fmt.Sprintf("BFEBFBFF%06X", r.Intn(16777216)))

	values.UUID = RandomGeneratedValue(strings.ToUpper(uuid.New().String()))

	values.Motherboard = RandomGeneratedValue(fmt.Sprintf("%s%d%s%s%s%s%04d%s%sMB",
		randomChoiceRand(r, []string{"A", "B", "C", "D", "E", "F", "S", "P"}),
		100+r.Intn(900),
		randomChoiceRand(r, []string{"N", "M", "T", "X", "Z"}),
		randomChoiceRand(r, []string{"R", "L", "K", "P"}),
		randomChoiceRand(r, []string{"C", "A", "B", "E"}),
		randomChoiceRand(r, []string{"X", "Y", "Z"}),
		r.Intn(10000),
		randomChoiceRand(r, []string{"F", "G", "H"}),
		randomChoiceRand(r, []string{"A", "B", "C", "D"})))

	values.BIOS = RandomGeneratedValue(fmt.Sprintf("%s%d%s%s%s%s%04d",
		randomChoiceRand(r, []string{"A", "B", "C", "D", "E", "F", "S", "P"}),
		1+r.Intn(9),
		randomChoiceRand(r, []string{"N", "M", "T", "X", "Z"}),
		randomChoiceRand(r, []string{"R", "L", "K", "P"}),
		randomChoiceRand(r, []string{"C", "A", "B", "E"}),
		randomChoiceRand(r, []string{"X", "Y", "Z"}),
		r.Intn(10000)))

	values.MachineGUID = RandomGeneratedValue(fmt.Sprintf("%06x%s%s%03x-%04x-%04x-%04x-%06x%02x",
		r.Intn(16777216),
		randomChoiceRand(r, []string{"a", "b", "c", "d", "e", "f"}),
		randomChoiceRand(r, []string{"5", "6", "7", "8"}),
		r.Intn(4096),
		r.Intn(4096),
		r.Intn(4096),
		r.Intn(4096),
		r.Intn(16777216),
		r.Intn(256)))

	// Generates the current datetime in YYYYMMDDHHMMSS format
	values.InstallDate = RandomGeneratedValue(time.Now().Format("20060102150405"))

	values.OSSerial = RandomGeneratedValue(fmt.Sprintf("%05d-%05d-%05d",
		10000+r.Intn(90000),
		10000+r.Intn(90000),
		10000+r.Intn(90000)))

	values.SID = RandomGeneratedValue(fmt.Sprintf("S-1-5-21-%d-%d-%d-%d",
		1000000000+r.Intn(9000000000),
		1000000000+r.Intn(9000000000),
		1000000000+r.Intn(9000000000),
		1000+r.Intn(9000)))

	macParts := make([]string, 6)
	for i := range macParts {
		macParts[i] = fmt.Sprintf("%02x", r.Intn(256))
	}

	// Fix the first byte of the MAC address to be a locally administered, unicast address
	// Set the second least significant bit to 1 (0x02) and clear the least significant bit (0x00)
	firstByteVal := r.Intn(256)
	firstByteVal = (firstByteVal & 0xfe) | 0x02 // Clear LSB and set second LSB
	macParts[0] = fmt.Sprintf("%02x", firstByteVal)

	values.MAC = RandomGeneratedValue(strings.Join(macParts, ":"))

	values.WLANGUID = RandomGeneratedValue(strings.ToUpper(uuid.New().String()))

	identifierBytes := make([]byte, 6)

	_, err := rand2.Read(identifierBytes)
	if err != nil {
		values.BSSID = "AB:AB:AB:AB:AB:AB"
	}
	hexParts := make([]string, 6)
	for i, b := range identifierBytes {
		hexParts[i] = fmt.Sprintf("%02X", b)
	}
	values.BSSID = RandomGeneratedValue(strings.Join(hexParts, ":"))

	diskSerialParts := make([]string, 8)
	for i := range diskSerialParts {
		diskSerialParts[i] = fmt.Sprintf("%04X", r.Intn(65536))
	}
	values.DiskSerial = RandomGeneratedValue(strings.Join(diskSerialParts, "_") + ".")

	values.VolumeSerial = RandomGeneratedValue(fmt.Sprintf("%d", r.Uint32()))

	values.DiskModel = RandomGeneratedValue(fmt.Sprintf("%s %s %s-%dG",
		randomStringRand(r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 4),
		randomStringRand(r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 2),
		randomStringRand(r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 4),
		100+r.Intn(900)))

	return values
}

// randomChoiceRand selects a random element from a string slice using the provided random source
func randomChoiceRand(r *rand.Rand, choices []string) string {
	return choices[r.Intn(len(choices))]
}

// randomStringRand generates a random string of length n from the given character set using the provided random source
func randomStringRand(r *rand.Rand, charset string, n int) string {
	result := make([]byte, n)
	for i := range result {
		result[i] = charset[r.Intn(len(charset))]
	}
	return string(result)
}
