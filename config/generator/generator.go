package config_generator

import (
	"fmt"
	"math/rand"
	"strings"
	"time"

	"github.com/google/uuid"
)

type RandomGeneratedValues struct {
	Hostname     string `json:"hostname"`
	ProcessorID  string `json:"processor_id"`
	UUID         string `json:"uuid"`
	Motherboard  string `json:"motherboard"`
	BIOS         string `json:"bios"`
	MachineGUID  string `json:"machineguid"`
	InstallDate  string `json:"installdate"`
	OSSerial     string `json:"osserial"`
	SID          string `json:"sid"`
	MAC          string `json:"mac"`
	WLANGUID     string `json:"wlanguid"`
	BSSID        string `json:"bssid"`
	DiskSerial   string `json:"diskserial"`
	VolumeSerial string `json:"volumeserial"`
	DiskModel    string `json:"diskmodel"`
}

func GenerateRandomValues() RandomGeneratedValues {
	source := rand.NewSource(time.Now().UnixNano())
	r := rand.New(source)

	values := RandomGeneratedValues{}

	// Generates a hostname like PC-012345
	values.Hostname = fmt.Sprintf("PC-%06d", r.Intn(1000000))

	// Generates a Processor ID in a specific hexadecimal format
	values.ProcessorID = fmt.Sprintf("BFEBFBFF%06X", r.Intn(16777216))

	// Generates a standard random UUID (GUID) in uppercase
	values.UUID = strings.ToUpper(uuid.New().String())

	// Generates a random string mimicking a motherboard serial/model format
	values.Motherboard = fmt.Sprintf("%s%d%s%s%s%s%04d%s%sMB",
		randomChoiceRand(r, []string{"A", "B", "C", "D", "E", "F", "S", "P"}),
		100+r.Intn(900),
		randomChoiceRand(r, []string{"N", "M", "T", "X", "Z"}),
		randomChoiceRand(r, []string{"R", "L", "K", "P"}),
		randomChoiceRand(r, []string{"C", "A", "B", "E"}),
		randomChoiceRand(r, []string{"X", "Y", "Z"}),
		r.Intn(10000),
		randomChoiceRand(r, []string{"F", "G", "H"}),
		randomChoiceRand(r, []string{"A", "B", "C", "D"}))

	// Generates a random string mimicking a BIOS version format
	values.BIOS = fmt.Sprintf("%s%d%s%s%s%s%04d",
		randomChoiceRand(r, []string{"A", "B", "C", "D", "E", "F", "S", "P"}),
		1+r.Intn(9),
		randomChoiceRand(r, []string{"N", "M", "T", "X", "Z"}),
		randomChoiceRand(r, []string{"R", "L", "K", "P"}),
		randomChoiceRand(r, []string{"C", "A", "B", "E"}),
		randomChoiceRand(r, []string{"X", "Y", "Z"}),
		r.Intn(10000))

	// Generates a random string mimicking a Windows MachineGuid format
	values.MachineGUID = fmt.Sprintf("%06x%s%s%03x-%04x-%04x-%04x-%06x%02x",
		r.Intn(16777216),
		randomChoiceRand(r, []string{"a", "b", "c", "d", "e", "f"}),
		randomChoiceRand(r, []string{"5", "6", "7", "8"}),
		r.Intn(4096),
		r.Intn(4096),
		r.Intn(4096),
		r.Intn(4096),
		r.Intn(16777216),
		r.Intn(256))

	// Generates the current datetime in YYYYMMDDHHMMSS format
	values.InstallDate = time.Now().Format("20060102150405")

	// Generates a random string mimicking an OS serial key format
	values.OSSerial = fmt.Sprintf("%05d-%05d-%05d",
		10000+r.Intn(90000),
		10000+r.Intn(90000),
		10000+r.Intn(90000))

	// Generates a random string mimicking a Windows Security Identifier (SID) format
	values.SID = fmt.Sprintf("S-1-5-21-%d-%d-%d-%d",
		1000000000+r.Intn(9000000000),
		1000000000+r.Intn(9000000000),
		1000000000+r.Intn(9000000000),
		1000+r.Intn(9000))

	// Generates a random MAC address in colon-separated hexadecimal format
	macParts := make([]string, 6)
	for i := range macParts {
		macParts[i] = fmt.Sprintf("%02x", r.Intn(256))
	}

	// Fix the first byte of the MAC address to be a locally administered, unicast address
	// Set the second least significant bit to 1 (0x02) and clear the least significant bit (0x00)
	firstByteVal := r.Intn(256)
	firstByteVal = (firstByteVal & 0xfe) | 0x02 // Clear LSB and set second LSB
	macParts[0] = fmt.Sprintf("%02x", firstByteVal)

	values.MAC = strings.Join(macParts, ":")

	// Generates a standard random UUID (GUID) in lowercase for Wireless LAN
	values.WLANGUID = uuid.New().String()

	// Generates a random BSSID in hyphen-separated hexadecimal format
	bssidParts := make([]string, 6)
	for i := range bssidParts {
		bssidParts[i] = fmt.Sprintf("%02X", r.Intn(256))
	}
	values.BSSID = strings.Join(bssidParts, "-")

	// Generates a random string mimicking a disk serial number format
	diskSerialParts := make([]string, 8)
	for i := range diskSerialParts {
		diskSerialParts[i] = fmt.Sprintf("%04X", r.Intn(65536))
	}
	values.DiskSerial = strings.Join(diskSerialParts, "_") + "."

	// Generates a random string mimicking a volume serial number
	values.VolumeSerial = fmt.Sprintf("%d", r.Uint32())

	// Generates a random string mimicking a disk model format
	values.DiskModel = fmt.Sprintf("%s %s %s-%dG",
		randomStringRand(r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 4),
		randomStringRand(r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 2),
		randomStringRand(r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 4),
		100+r.Intn(900))

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
