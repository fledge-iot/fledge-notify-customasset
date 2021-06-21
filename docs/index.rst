.. Images
.. |customasset_1| image:: images/customasset_1.jpg

CustomAsset Notification
==================

The *fledge-notify-customasset* notification delivery plugin is unusually in that it does not notify an external system, instead it creates a new customasset which is then processed like any other customasset within Fledge. This plugin is useful to inform up stream systems that a event has occurred and allow them to take action or merely as a way to have a record of a condition occurring which may not require any further actions.

Once you have created your notification rule and move on to the delivery mechanism

  - Select the customasset plugin from the list of plugins

  - Click *Next*

    +-----------+
    | |customasset_1| |
    +-----------+

  - Now configure the customasset delivery plugin

    - **CustomAsset**: The name of the customasset to create.

    - **Description**: A textual description to add to the customasset

  - Enable the plugin and click *Next*

  - Complete your notification setup

The customasset that will be created when the notification triggers will contain

  - The timestamp of the trigger event

  - Three data points

    - **rule**: The name of the notification that triggered this customasset creation

    - **description**: The textual description entered in the configuration of the delivery plugin

    - **event**: This will be one of *triggered* or *cleared*. If the notification type was not set to be *toggled* then the *cleared* event will not appear. If *toggled* was set as the notification type then there will be a *triggered* value in the customasset created when the rule triggered and a *cleared* value in the customasset generated when the rule moved from the triggered to untriggered state.
