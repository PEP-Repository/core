import json
import hashlib
from datetime import datetime
import argparse

def map_severity(infer_severity):
    mapping = {
        "ERROR": "High",
        "WARNING": "Medium",
        "INFO": "Low"
    }
    return mapping.get(infer_severity, "Unknown")

def generate_fingerprint(issue):
    """
    Generate a stable fingerprint for deduplication.
    Uses file, line, bug_type, and key from Infer.
    """
    data = f"{issue.get('file')}|{issue.get('line')}|{issue.get('column')}|{issue.get('key')}"
    return hashlib.sha256(data.encode("utf-8")).hexdigest()

def main():
    parser = argparse.ArgumentParser(description="Convert Infer JSON report to GitLab SAST report")
    parser.add_argument("-i", "--input", required=True, help="Path to Infer JSON report")
    parser.add_argument("-o", "--output", required=True, help="Path to output GitLab SAST report")
    args = parser.parse_args()

    with open(args.input) as f:
        infer_data = json.load(f)

    vulnerabilities = []

    for issue in infer_data:
        description = issue.get("qualifier", "")
        suggestion = issue.get("suggestion", "")
        copy_type = issue.get("extras", {}).get("copy_type", "")
        full_description = description
        if suggestion:
            full_description += "\nRemediation: " + suggestion
        if copy_type:
            full_description += f"\nCopy type: {copy_type}"

        evidence = []
        for trace in issue.get("bug_trace", []):
            evidence.append({
                "file": trace.get("filename"),
                "start_line": trace.get("line_number"),
                "start_column": trace.get("column_number"),
                "description": trace.get("description"),
                "level": trace.get("level")
            })

        vuln = {
            "id": generate_fingerprint(issue),
            "category": "sast",
            "name": issue.get("bug_type_hum", issue.get("bug_type", "Infer Issue")),
            "message": issue.get("qualifier", ""),
            "description": full_description,
            "severity": map_severity(issue.get("severity")),
            "confidence": "Medium",
            "scanner": {
                "id": "infer",
                "name": "Infer"
            },
            "location": {
                "file": issue.get("file"),
                "start_line": issue.get("line"),
                "start_column": issue.get("column"),
                "method": issue.get("procedure")
            },
            "identifiers": [
                {
                    "type": "infer_type",
                    "name": issue.get("bug_type"),
                    "value": issue.get("key")
                }
            ],
            "evidence": evidence
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

    with open(args.output, "w") as f:
        json.dump(report, f, indent=2)

    print(f"Converted {args.input} → {args.output} successfully. {len(vulnerabilities)} issues processed.")

if __name__ == "__main__":
    main()