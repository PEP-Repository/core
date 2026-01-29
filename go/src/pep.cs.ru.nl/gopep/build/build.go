package build

const MANUAL_PROTOCOL_CHECKSUM_COMPONENT uint8 = 2 // Increase when client+server software need to be upgraded in a big bang. Cycle back from 255 to 1 (or 0) if necessary

var (
	BuildProjectPath  string
	BuildReference    string
	BuildMajorVersion string
	BuildMinorVersion string
	BuildPipelineId   string
	BuildJobId        string
	BuildCommit       string
	BuildTarget       string
)
