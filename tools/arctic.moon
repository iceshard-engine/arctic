import Application from require 'ice.application'

import UpdateCommand from require 'ice.commands.update'
import BuildCommand from require 'ice.commands.build'
import VStudioCommand from require 'ice.commands.vstudio'

class Arctic extends Application
    @name: 'Arctic'
    @commands: {
        'build': BuildCommand
        'update': UpdateCommand
        'vstudio': VStudioCommand
    }

    -- Plain call to the application
    execute: (args) =>
        print "#{@@name} project tool - v0.1-alpha"
        print ' For more options see the -h,--help output.'

{ :Arctic }
