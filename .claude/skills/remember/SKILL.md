---
name: remember
description: Search through past conversation histories to recall previous discussions, decisions, and context. Use when you need to reference what was discussed or decided in previous sessions.
---

# Conversation History Search

Search through past conversation histories stored in `~/.claude/projects/*/*.jsonl` to help recall previous discussions, decisions, code explanations, and context from earlier sessions.

## Instructions

1. Run the search command with a query string:
   ```bash
   uv run python .claude/skills/remember/remember.py "<query>"
   ```

2. Optionally include agent conversations (sub-tasks):
   ```bash
   uv run python .claude/skills/remember/remember.py "<query>" --include-agents
   ```

## Features

- Searches both user and assistant messages
- Finds discussions about specific topics, features, or files
- Shows conversation context with timestamps
- Groups results by session for easy navigation
- Sorts by recency (most recent first)
- Skips agent conversations by default (use `--include-agents` to include them)
- Case-insensitive substring matching

## Examples

Search for discussions about networking:
```bash
uv run python .claude/skills/remember/remember.py "networking"
```

Find conversations about a specific file:
```bash
uv run python .claude/skills/remember/remember.py "DESIGN.md"
```

Look for combat system discussions:
```bash
uv run python .claude/skills/remember/remember.py "combat system"
```

Search including agent conversations:
```bash
uv run python .claude/skills/remember/remember.py "ENet" --include-agents
```

## When to Use This Skill

- Need to recall why a design decision was made
- Want to find previous discussions about a feature
- Looking for context about how something was implemented
- Trying to remember what was decided in a past conversation
- Need to reference code explanations from earlier sessions

## Notes

- Searches conversation histories in `~/.claude/projects/*/`
- By default, excludes agent conversations (background tasks)
- Results are truncated to 200 characters for readability
- Shows file path and line number for reference
- Groups results by session to see conversation flow
- Displays up to 50 most recent matches by default
