import json
import boto3
from decimal import Decimal

dynamodb = boto3.resource("dynamodb", region_name="us-west-2")

class DecimalEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, Decimal):
            return float(o)
        return super().default(o)

def handler(event, context):
    headers = {
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type",
        "Content-Type": "application/json"
    }

    method = event.get("requestContext", {}).get("http", {}).get("method", "")
    if method == "OPTIONS":
        return {"statusCode": 200, "headers": headers, "body": ""}

    path = event.get("rawPath", "/")

    if path == "/telemetry":
        table = dynamodb.Table("ggl-demo-telemetry")
        resp = table.query(
            KeyConditionExpression="device_id = :d",
            ExpressionAttributeValues={":d": "cam-3"},
            ScanIndexForward=False,
            Limit=60
        )
        return {"statusCode": 200, "headers": headers,
                "body": json.dumps(resp["Items"], cls=DecimalEncoder)}

    elif path == "/insights":
        table = dynamodb.Table("ggl-demo-insights")
        resp = table.query(
            KeyConditionExpression="device_id = :d",
            ExpressionAttributeValues={":d": "cam-3"},
            ScanIndexForward=False,
            Limit=10
        )
        return {"statusCode": 200, "headers": headers,
                "body": json.dumps(resp["Items"], cls=DecimalEncoder)}

    return {"statusCode": 404, "headers": headers,
            "body": json.dumps({"error": "not found"})}
