import json
import os
import time
import boto3

bedrock = boto3.client("bedrock-runtime", region_name="us-west-2")
dynamodb = boto3.resource("dynamodb", region_name="us-west-2")
insights_table = dynamodb.Table("ggl-demo-insights")

def handler(event, context):
    temp = event.get("temp", 0)
    device_id = event.get("device_id", "unknown")
    mode = event.get("mode", "normal")
    ts = event.get("timestamp", int(time.time()))

    # Throttle: only trigger on state change (anomaly→recovery or normal→anomaly)
    is_recovery = (mode == "low-power" and temp < 32)
    resp = insights_table.query(
        KeyConditionExpression="device_id = :d",
        ExpressionAttributeValues={":d": device_id},
        ScanIndexForward=False,
        Limit=1
    )
    if resp["Items"]:
        last = resp["Items"][0]
        last_was_recovery = float(last.get("temp", 99)) < 35
        if last_was_recovery == is_recovery:
            return {"statusCode": 200, "insight": "no state change"}

    if is_recovery:
        prompt = (
            f"You are an IoT monitoring system. Camera device '{device_id}' "
            f"was overheating but a low-power mode was deployed remotely. "
            f"Temperature is now {temp}°C and recovering to normal range (20-35°C). "
            f"In 1-2 sentences, confirm the recovery and note the fix worked."
        )
    else:
        prompt = (
            f"You are an IoT monitoring system. Camera device '{device_id}' "
            f"reported temperature {temp}°C. Normal range is 20-35°C. "
            f"In 1-2 sentences, describe the issue and recommend an action. "
            f"Be specific and concise."
        )

    response = bedrock.invoke_model(
        modelId="anthropic.claude-3-5-haiku-20241022-v1:0",
        body=json.dumps({
            "anthropic_version": "bedrock-2023-05-31",
            "max_tokens": 150,
            "messages": [{"role": "user", "content": prompt}]
        }),
        contentType="application/json",
    )

    body = json.loads(response["body"].read())
    insight = body["content"][0]["text"]

    insights_table.put_item(Item={
        "device_id": device_id,
        "timestamp": int(ts),
        "temp": str(temp),
        "insight": insight,
    })

    return {"statusCode": 200, "insight": insight}
