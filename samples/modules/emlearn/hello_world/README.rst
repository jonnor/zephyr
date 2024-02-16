.. _emlearn_hello_world:

emlearn Hello World sample
########################################

Overview
********


Building and Running
********************

The sample should work on most boards since it does not rely on any sensors.

The reference kernel application can be built and executed on QEMU as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/modules/emlearn/hello_world
   :host-os: unix
   :board: qemu_x86
   :goals: run
   :compact:

Exit QEMU by pressing :kbd:`CTRL+A` :kbd:`x`.


Sample Output
=============

.. code-block:: console

    ...

    EXAMPLE

    ...


Modifying Sample for Your Own Project
*************************************

emlearn is easy to integrate into any project.
You can start with this sample, or a sample that has the sensors you are interested in.

To build with emlearn,
you must enable the below Kconfig options in your :file:`prj.conf`:

.. code-block:: kconfig

    CONFIG_EMLEARN=y


Training
********
Follow the instructions in the :file:`train/` directory to train your
own model for use in the sample.
