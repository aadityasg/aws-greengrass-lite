import json
import boto3

gg = boto3.client("greengrassv2", region_name="us-west-2")

THING_GROUP_ARN = "arn:aws:iot:us-west-2:334833665744:thinggroup/demo_ggl_001_group"

def handler(event, context):
    body = {}
    try:
        body = json.loads(event.get("body", "{}") or "{}")
    except Exception:
        pass

    mode = body.get("mode", "low-power")
    version = "1.0.1" if mode == "low-power" else "1.0.0"

    response = gg.create_deployment(
        targetArn=THING_GROUP_ARN,
        deploymentName=f"genai-demo-{mode}",
        components={
            "com.demo.SensorSim": {
                "componentVersion": version,
            }
        }
    )
    return {
        "statusCode": 200,
        "headers": {
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
            "Access-Control-Allow-Headers": "Content-Type",
            "Content-Type": "application/json"
        },
        "body": json.dumps({
            "deploymentId": response["deploymentId"],
            "message": f"Deployed {mode} mode (v{version})"
        })
    }
