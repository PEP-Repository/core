import json
import uuid
from datetime import datetime
import argparse

def map_severity(infer_severity):
    mapping = {
        "ERROR": "High",
        "WARNING": "Medium",
        "INFO": "Low"
    }
    return mapping.get(infer_severity, "Unknown")

def main():
    parser = argparse.ArgumentParser(description="Convert Infer JSON report to GitLab SAST report")
    parser.add_argument("-i", "--input", required=True, help="Path to Infer JSON report")
    parser.add_argument("-o", "--output", required=True, help="Path to output GitLab SAST report")
    args = parser.parse_args()

    # Load Infer report
    with open(args.input) as f:
        infer_data = json.load(f)

    vulnerabilities = []

    for issue in infer_data:
        vuln = {
            "id": str(uuid.uuid4()),
            "category": "sast",
            "name": issue.get("bug_type", "Infer Issue"),
            "message": issue.get("qualifier", ""),
            "description": issue.get("qualifier", ""),
            "severity": map_severity(issue.get("severity")),
            "confidence": "Medium",
            "scanner": {
                "id": "infer",
                "name": "Infer"
            },
            "location": {
                "file": issue.get("file"),
                "start_line": issue.get("line")
            },
            "identifiers": [
                {
                    "type": "infer_type",
                    "name": issue.get("bug_type"),
                    "value": issue.get("bug_type")
                }
            ]
        }
        vulnerabilities.append(vuln)

    report = {
        "version": "15.0.0",
        "scan": {
            "type": "sast",
            "status": "success",
            "start_time": datetime.utcnow().isoformat() + "Z",
            "end_time": datetime.utcnow().isoformat() + "Z",
            "scanner": {
                "id": "infer",
                "name": "Infer"
            }
        },
        "vulnerabilities": vulnerabilities
    }

    # Write GitLab SAST report
    with open(args.output, "w") as f:
        json.dump(report, f, indent=2)

    print(f"Converted {args.input} → {args.output} successfully.")

if __name__ == "__main__":
    main()