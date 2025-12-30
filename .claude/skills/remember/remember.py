#!/usr/bin/env python3
"""
Conversation history search tool for Claude Code.

Searches through past conversation histories stored in ~/.claude/projects/*/*.jsonl
to help recall previous discussions, decisions, and context.
"""

import json
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import List, Optional


@dataclass
class ConversationMatch:
    """Represents a single conversation match."""
    session_id: str
    timestamp: str
    role: str  # 'user' or 'assistant'
    content: str
    file_path: Path
    line_num: int

    def format_timestamp(self) -> str:
        """Format timestamp to human-readable form."""
        try:
            dt = datetime.fromisoformat(self.timestamp.replace('Z', '+00:00'))
            return dt.strftime('%Y-%m-%d %H:%M:%S')
        except:
            return self.timestamp

    def truncate_content(self, max_length: int = 200) -> str:
        """Truncate content to a reasonable length."""
        if len(self.content) <= max_length:
            return self.content
        return self.content[:max_length] + "..."

    def __str__(self):
        formatted_time = self.format_timestamp()
        truncated = self.truncate_content()
        # Replace newlines with spaces for compact display
        one_line = truncated.replace('\n', ' ').strip()
        return f"[{formatted_time}] {self.role}: {one_line}"


class ConversationSearch:
    """Search through conversation histories."""

    def __init__(self, projects_dir: Path = None):
        if projects_dir is None:
            projects_dir = Path.home() / '.claude' / 'projects'
        self.projects_dir = projects_dir

    def extract_text_from_content(self, content) -> str:
        """
        Extract text from message content.
        Content can be a string or a list of content blocks.
        """
        if isinstance(content, str):
            return content
        elif isinstance(content, list):
            text_parts = []
            for block in content:
                if isinstance(block, dict):
                    if block.get('type') == 'text':
                        text_parts.append(block.get('text', ''))
                    elif block.get('type') == 'tool_use':
                        # Include tool name for context
                        tool_name = block.get('name', 'unknown')
                        text_parts.append(f"[Tool: {tool_name}]")
                    elif block.get('type') == 'tool_result':
                        # Skip tool results as they can be very long
                        text_parts.append("[Tool result]")
                elif isinstance(block, str):
                    text_parts.append(block)
            return ' '.join(text_parts)
        return str(content)

    def search_conversations(self, query: str, include_agents: bool = False) -> List[ConversationMatch]:
        """
        Search for query string in conversation histories.

        Args:
            query: The search string (treated as case-insensitive substring match)
            include_agents: Whether to include agent conversations (default: False)
        """
        matches = []

        # Compile regex pattern for case-insensitive search
        pattern = re.compile(re.escape(query), re.IGNORECASE)

        # Search all project directories
        if not self.projects_dir.exists():
            print(f"Projects directory not found: {self.projects_dir}")
            return matches

        for project_dir in self.projects_dir.iterdir():
            if not project_dir.is_dir():
                continue

            # Search all JSONL files in the project
            for jsonl_file in project_dir.glob('*.jsonl'):
                # Skip agent files unless requested
                if not include_agents and jsonl_file.name.startswith('agent-'):
                    continue

                try:
                    with open(jsonl_file, 'r', encoding='utf-8') as f:
                        for line_num, line in enumerate(f, 1):
                            try:
                                data = json.loads(line)

                                # Skip non-message entries
                                msg_type = data.get('type')
                                if msg_type not in ('user', 'assistant'):
                                    continue

                                # Extract message content
                                message = data.get('message', {})
                                if isinstance(message, dict):
                                    content = message.get('content', '')
                                else:
                                    content = str(message)

                                # Extract text from content
                                text = self.extract_text_from_content(content)

                                # Check if query matches
                                if pattern.search(text):
                                    match = ConversationMatch(
                                        session_id=data.get('sessionId', 'unknown'),
                                        timestamp=data.get('timestamp', ''),
                                        role=msg_type,
                                        content=text,
                                        file_path=jsonl_file,
                                        line_num=line_num
                                    )
                                    matches.append(match)

                            except json.JSONDecodeError:
                                # Skip malformed lines
                                continue

                except Exception as e:
                    # Skip files that can't be read
                    print(f"Warning: Could not read {jsonl_file}: {e}", file=sys.stderr)
                    continue

        return matches

    def print_results(self, query: str, matches: List[ConversationMatch], max_results: int = 50):
        """Print search results in a readable format."""
        if not matches:
            print(f"No conversations found matching '{query}'")
            return

        # Sort by timestamp (most recent first)
        matches.sort(key=lambda m: m.timestamp, reverse=True)

        # Limit results
        if len(matches) > max_results:
            print(f"Found {len(matches)} matches (showing {max_results} most recent):\n")
            matches = matches[:max_results]
        else:
            print(f"Found {len(matches)} matches:\n")

        # Group by session
        by_session = defaultdict(list)
        for match in matches:
            by_session[match.session_id].append(match)

        # Print results grouped by session
        for session_id, session_matches in sorted(
            by_session.items(),
            key=lambda x: max(m.timestamp for m in x[1]),
            reverse=True
        ):
            print(f"SESSION: {session_id}")
            print("-" * 80)

            # Sort matches within session by timestamp
            session_matches.sort(key=lambda m: m.timestamp)

            for match in session_matches:
                print(f"  {match}")
                # Print file location for reference
                print(f"    → {match.file_path.name}:{match.line_num}")
            print()


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print("Usage: uv run python remember.py <query> [--include-agents]")
        print("\nExamples:")
        print("  uv run python remember.py 'networking'")
        print("  uv run python remember.py 'DESIGN.md' --include-agents")
        print("  uv run python remember.py 'combat system'")
        print("\nSearches through conversation histories in ~/.claude/projects/")
        print("Use --include-agents to also search agent conversations")
        sys.exit(1)

    query = sys.argv[1]
    include_agents = '--include-agents' in sys.argv

    # Create searcher and run search
    searcher = ConversationSearch()
    matches = searcher.search_conversations(query, include_agents=include_agents)
    searcher.print_results(query, matches)


if __name__ == '__main__':
    main()
