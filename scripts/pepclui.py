#!/usr/bin/env python3

from gooey import Gooey, GooeyParser

@Gooey(target='./pepcli', program_name='Data Administrator Interface', suppress_gooey_flag=True, show_success_modal=False, show_restart_button=False, required_cols=1)
def main():
  parser = GooeyParser(description="Perform data administration tasks")
  parser.add_argument('--oauth-token',
                      metavar='OAuth Token',
                      help='The OAuth token, provided by the PEP team, for the data administrator')
  subs = parser.add_subparsers(help="Commands")
  
  subs.add_parser('ama query', prog='Query', help='Show all columns, column groups and access rules')

  columnCreateParser = subs.add_parser('ama column create', prog='Create column', help='Create column')
  columnCreateParser.add_argument('column', help='The name of the column to create')

  columnRemoveParser = subs.add_parser('ama column remove', prog='Remove column', help='Remove column')
  columnRemoveParser.add_argument('column', help='The name of the column to remove')

  columnGroupCreateParser = subs.add_parser('ama columnGroup create', prog='Create column group', help='Create column group')
  columnGroupCreateParser.add_argument('column group', help='The name of the column group to create')

  columnGroupRemoveParser = subs.add_parser('ama columnGroup remove', prog='Remove column group', help='Remove column group')
  columnGroupRemoveParser.add_argument('column group', help='The name of the column group to remove')

  participantGroupCreateParser = subs.add_parser('ama group create', prog='Create participant group', help='Create participant group')
  participantGroupCreateParser.add_argument('participant group', help='The name of the participant group to create')

  participantGroupRemoveParser = subs.add_parser('ama group remove', prog='Remove participant group', help='Remove participant group')
  participantGroupRemoveParser.add_argument('participant group', help='The name of the participant group to remove')

  columnAddToParser = subs.add_parser('ama column addTo', prog='Add column to column group', help='Add column to column group')
  columnAddToParser.add_argument('column', help='The name of the column to add')
  columnAddToParser.add_argument('column group', help='The name of the column group to add to')

  columnRemoveFromParser = subs.add_parser('ama column removeFrom', prog='Remove column from column group', help='Remove column from column group')
  columnRemoveFromParser.add_argument('column', help='The name of the column to remove')
  columnRemoveFromParser.add_argument('column group', help='The name of the column group to remove from')

  participantAddToParser = subs.add_parser('ama group addTo', prog='Add participant to participant group', help='Add participant to participant group')
  participantAddToParser.add_argument('participant group', help='The name of the participant group to add to')
  participantAddToParser.add_argument('participant', help='The PEPid or polymorphic pseudonym of the participant to add')

  participantRemoveFromParser = subs.add_parser('ama group removeFrom', prog='Remove participant from participant group', help='Remove participant from participant group')
  participantRemoveFromParser.add_argument('participant group', help='The name of the participant group to remove from')
  participantRemoveFromParser.add_argument('participant', help='The PEPid or polymorphic pseudonym of the participant to remove')

  parser.parse_args()

if __name__ == "__main__":
    main()