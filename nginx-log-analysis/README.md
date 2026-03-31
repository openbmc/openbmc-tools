# Nginx Access Log Analyzer

A Python script to analyze nginx access logs and provide intelligent recommendations for blocking malicious bots and high-volume traffic sources.

## Features

- **Bot Detection**: Identifies known bot user agents (ClaudeBot, GPTBot, Baiduspider, etc.)
- **IP Range Aggregation**: Groups IPs into /24 and /16 CIDR ranges for efficient blocking
- **Traffic Analysis**: Analyzes request patterns, status codes, and login attempts
- **Smart Recommendations**: Provides actionable nginx configuration rules
- **Legitimate Bot Protection**: Excludes legitimate crawlers (Google, Bing, etc.) from blocking recommendations

## Requirements

- Python 3.6 or higher
- Standard library only (no external dependencies)

## Installation

```bash
# Make the script executable
chmod +x analyze-nginx-log
```

## Usage

### Basic Usage

```bash
./analyze-nginx-log access.log
```

### With Custom Threshold

```bash
# Only flag IPs/ranges with 500+ requests
./analyze-nginx-log access.log --threshold 500

# More aggressive - flag IPs with 2000+ requests
./analyze-nginx-log access.log --threshold 2000
```

### Save Report to File

```bash
./analyze-nginx-log access.log > report.txt
```

### Help

```bash
./analyze-nginx-log --help
```

## Output Sections

The script generates a comprehensive report with the following sections:

### 1. Top IPs by Request Count
Shows the most active IP addresses with:
- Total requests
- Percentage of total traffic
- Number of 403 (forbidden) responses
- Login attempt count

### 2. Bot Traffic Analysis
Detailed breakdown of bot traffic by type:
- ClaudeBot (Anthropic)
- GPTBot (OpenAI)
- Baiduspider (Baidu)
- AliyunSecBot (Alibaba)
- And more...

For each bot type, shows:
- Number of unique IPs
- Total requests and percentage
- Top /24 and /16 IP ranges

### 3. High-Volume IP Ranges
Non-bot traffic aggregated into CIDR ranges, useful for identifying:
- Scraper networks
- DDoS sources
- Geographic regions with excessive traffic

### 4. Login Attempt Analysis
IPs making excessive requests to `/login/` paths, which may indicate:
- Brute force attempts
- Credential stuffing
- Automated scanning

### 5. Status Code Summary
Overview of HTTP response codes to identify:
- Rate limiting (499)
- Forbidden access (403)
- Server errors (500, 502)

### 6. Blocking Recommendations
Ready-to-use nginx configuration snippets:
- IP range deny rules
- User-Agent blocking regex
- Individual IP blocks for excessive login attempts

## Example Output

```
================================================================================
BLOCKING RECOMMENDATIONS
================================================================================

1. BOT TRAFFIC TO BLOCK:
--------------------------------------------------------------------------------
  deny 216.73.216.0/24       # ClaudeBot - 70,281 requests (3.97%)
  deny 57.141.0.0/24         # meta-externalagent - 5,319 requests (0.30%)

2. HIGH-VOLUME IP RANGES TO CONSIDER:
--------------------------------------------------------------------------------
  deny 220.181.51.0/24       # 17,831 requests (1.01%) - 8 IPs

3. USER-AGENT BLOCKING:
--------------------------------------------------------------------------------
  if ($http_user_agent ~* "(ClaudeBot|GPTBot|Baiduspider|AliyunSecBot)") {
      return 403;
  }

4. INDIVIDUAL IPs WITH EXCESSIVE LOGIN ATTEMPTS:
--------------------------------------------------------------------------------
  deny 66.249.64.39          # 8,064 login attempts
```

## Applying Recommendations

### Step 1: Review the Report
Carefully review all recommendations. Some high-volume IPs may be legitimate users.

### Step 2: Test in Staging
Apply the rules to a staging environment first:

```nginx
# Add to your nginx.conf or site config
location / {
    # Bot IP ranges
    deny 216.73.216.0/24;
    deny 57.141.0.0/24;

    # User-Agent blocking
    if ($http_user_agent ~* "(ClaudeBot|GPTBot|Baiduspider)") {
        return 403;
    }

    # ... rest of your config
}
```

### Step 3: Monitor
After applying rules:
- Check nginx error logs for any issues
- Monitor legitimate user complaints
- Verify bot traffic reduction

### Step 4: Reload Nginx
```bash
sudo nginx -t  # Test configuration
sudo systemctl reload nginx
```

## Configuration Options

### Threshold Parameter
The `--threshold` parameter controls sensitivity:

- **Low (500-1000)**: More aggressive, catches more bots but higher false positive risk
- **Medium (1000-2000)**: Balanced approach (default: 1000)
- **High (2000+)**: Conservative, only blocks very high-volume sources

### Customizing Bot Patterns
Edit the `BOT_PATTERNS` list in the script to add/remove bot user agents:

```python
BOT_PATTERNS = [
    r'ClaudeBot', r'GPTBot', r'ChatGPT', r'Amazonbot',
    r'YourCustomBot',  # Add your patterns here
    # ...
]
```

### Protecting Legitimate Bots
The script automatically excludes these from blocking recommendations:
- Googlebot
- Bingbot
- Slurp (Yahoo)
- DuckDuckBot
- Applebot

To modify, edit the `LEGITIMATE_BOTS` list in the script.

## Performance

- Processes ~100,000 log lines per second
- Memory efficient (streaming parser)
- Handles multi-million line log files

Example: 1.77 million lines analyzed in ~18 seconds

## Troubleshooting

### "Log file not found"
Ensure the path to your access.log is correct:
```bash
ls -lh /var/log/nginx/access.log
```

### "Permission denied"
You may need sudo to read nginx logs:
```bash
sudo python3 analyze-nginx-log /var/log/nginx/access.log
```

### Unexpected Results
- Verify your nginx log format matches the standard combined format
- Check if log rotation has occurred (analyze multiple files)
- Adjust threshold if too many/few recommendations

## Log Format Support

The script expects standard nginx combined log format:
```
$remote_addr - - [$time_local] "$request" $status $body_bytes_sent "$http_referer" "$http_user_agent"
```

Example:
```
192.168.1.1 - - [30/Mar/2026:00:00:36 -0500] "GET /index.html HTTP/1.1" 200 1234 "-" "Mozilla/5.0..."
```

## Best Practices

1. **Run Regularly**: Analyze logs weekly or after traffic spikes
2. **Incremental Blocking**: Start with obvious bots, then expand
3. **Document Changes**: Keep track of what you block and why
4. **Monitor Impact**: Check traffic metrics after applying rules
5. **Whitelist Important IPs**: Ensure critical services aren't blocked
6. **Use Rate Limiting**: Consider rate limiting before outright blocking

## Advanced Usage

### Analyze Multiple Log Files
```bash
cat access.log access.log.1 | python3 analyze-nginx-log /dev/stdin
```

### Filter by Date Range
```bash
grep "30/Mar/2026" access.log | python3 analyze-nginx-log /dev/stdin
```

### Compare Before/After
```bash
# Before blocking
./analyze-nginx-log access.log.before > before.txt

# After blocking
./analyze-nginx-log access.log.after > after.txt

# Compare
diff before.txt after.txt
```

## Contributing

Feel free to enhance the script with:
- Additional bot patterns
- Custom log format parsers
- Geographic IP analysis
- Time-based pattern detection

## License

This script is provided as-is for analyzing nginx logs and improving server security.

## Support

For issues or questions:
1. Check the troubleshooting section
2. Verify your log format
3. Review the example output
4. Adjust threshold parameters

---

**Note**: Always test blocking rules in a non-production environment first. Overly aggressive blocking can impact legitimate users.
